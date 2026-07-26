#include "mcp2518fd_can.h"
#include "mcp2518fd_registers.h"
#include "mcp2518fd_timing.h"

MCP2518Driver::MCP2518Driver(SPIClass& spi, uint8_t csPin, int8_t intPin)
    : mSpi(spi, csPin), mFsys(0), mTxTimeoutMs(10), mNbtcfg(0), mIntPin(intPin), mRxPending(false)
{
}

// ----------------------------------------------------------------------------
// Public API

CanStatus MCP2518Driver::configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode)
{
    return configure(nominalBps, dataBps, mode, CanConfig{});
}

CanStatus MCP2518Driver::configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, const CanConfig& cfg)
{
    mSpi.begin();
    // If the chip is in Sleep mode (OSCDIS=1), clear it before reset
    // so the oscillator is running when reset() is issued
    if (mSpi.read32(REG_OSC) & OSC_OSCDIS)
    {
        uint8_t osc0 = mSpi.read8(REG_OSC);
        mSpi.write8(REG_OSC, osc0 & ~(uint8_t)OSC_OSCDIS);
        delay(5);  // oscillator stabilisation (max 3 ms, DS20006027B page 79)
    }
    mSpi.reset();
    delay(20);
    mSpi.setMode(MODE_CONFIG);

    mFsys = detectFsys();
    if (mFsys == 0) return CanStatus::CLOCK_NOT_READY;

    uint32_t nbtcfg, dbtcfg, tdcfg;
    // In MODE_CLASSIC the data phase is unused; pass nominal rate for both so
    // calcBitTiming() succeeds and the data registers get a consistent value.
    uint32_t effectiveDataBps = (mode == MODE_CLASSIC) ? nominalBps : dataBps;
    if (!calcBitTiming(mFsys, nominalBps, effectiveDataBps, nbtcfg, dbtcfg, tdcfg))
        return CanStatus::RATE_NOT_ACHIEVABLE;

    applyTiming(nbtcfg, dbtcfg, tdcfg);
    configFifos(cfg.rxFifoDepth, cfg.enableTimestamp);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(mFsys, nbtcfg, dbtcfg);
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;

    // Enable TBC after mode transition — CiTSCON is not config-mode-only
    // and must be set after exiting config mode to survive the transition
    if (cfg.enableTimestamp)
        mSpi.write32(REG_CiTSCON, TSCON_TBCEN);

    if (mIntPin >= 0)
    {
        sIsrInstance = this;
        attachInterrupt(digitalPinToInterrupt(mIntPin), sIsrHandler, FALLING);
    }

    return CanStatus::OK;
}

CanStatus MCP2518Driver::setDataRate(uint32_t dataBps)
{
    if (mSpi.getMode() == MODE_CLASSIC) return CanStatus::INVALID_MODE;

    uint8_t prevMode = mSpi.getMode();

    // Calculate before entering config mode — fail early without disturbing the chip
    uint32_t nBrp   = (mNbtcfg >> 24) & 0xFF;
    uint32_t nTseg1 = (mNbtcfg >> 16) & 0xFF;
    uint32_t nTseg2 = (mNbtcfg >>  8) & 0x7F;
    uint32_t nominalBps = mFsys / ((nBrp + 1) * (1 + nTseg1 + nTseg2));

    uint32_t nbtcfg, dbtcfg, tdcfg;
    if (!calcBitTiming(mFsys, nominalBps, dataBps, nbtcfg, dbtcfg, tdcfg))
        return CanStatus::RATE_NOT_ACHIEVABLE;  // chip state unchanged

    mSpi.setMode(MODE_CONFIG);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(mFsys, mNbtcfg, dbtcfg);

    if (!mSpi.setMode(prevMode)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

CanStatus MCP2518Driver::configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode)
{
    return configureRaw(nbtcfg, dbtcfg, tdcfg, mode, CanConfig{});
}

CanStatus MCP2518Driver::configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode, const CanConfig& cfg)
{
    mSpi.begin();
    mSpi.reset();
    delay(20);
    mSpi.setMode(MODE_CONFIG);

    applyTiming(nbtcfg, dbtcfg, tdcfg);
    configFifos(cfg.rxFifoDepth, cfg.enableTimestamp);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(0, nbtcfg, dbtcfg);  // fsys unknown in raw path
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;

    if (cfg.enableTimestamp)
        mSpi.write32(REG_CiTSCON, TSCON_TBCEN);

    return CanStatus::OK;
}

CanStatus MCP2518Driver::setDataBitTimingRaw(uint32_t dbtcfg, uint32_t tdcfg)
{
    uint8_t prevMode = mSpi.getMode();
    mSpi.setMode(MODE_CONFIG);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(mFsys, mNbtcfg, dbtcfg);
    if (!mSpi.setMode(prevMode)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

CanTxResult MCP2518Driver::transmit(const CanMsg& msg)
{
    // Listen Only mode: chip ignores TXREQ, no ACK sent — return immediately
    if (mSpi.getMode() == MODE_LISTEN) return CanTxResult::NoAck;

    // Classic CAN mode: FD frames are not permitted
    if (mSpi.getMode() == MODE_CLASSIC && msg.fdf) return CanTxResult::InvalidMode;
    if (!(mSpi.read32(FIFO_STA(1)) & FIFOSTA_TFNRFNIF))
        return CanTxResult::FifoFull;

    uint16_t addr = txRamAddr();

    uint32_t t0, t1;
    if (msg.ext)
    {
        t0 = encodeEidT0(msg.id);
        t1 = (1u << 4)
           | ((msg.fdf ? 1u : 0u) << 7)
           | ((msg.brs ? 1u : 0u) << 6)
           | (msg.dlc & 0xFu);
    }
    else
    {
        t0 = msg.id & 0x7FFu;
        t1 = ((msg.fdf ? 1u : 0u) << 7)
           | ((msg.brs ? 1u : 0u) << 6)
           | (msg.dlc & 0xFu);
    }

    mSpi.write32(addr,     t0);
    mSpi.write32(addr + 4, t1);

    uint8_t len = dlcToLen(msg.dlc);
    for (uint8_t i = 0; i < len; i += 4)
    {
        uint32_t w = (uint32_t)msg.data[i]
                   | ((uint32_t)msg.data[i+1] << 8)
                   | ((uint32_t)msg.data[i+2] << 16)
                   | ((uint32_t)msg.data[i+3] << 24);
        mSpi.write32(addr + 8 + i, w);
    }

    mSpi.write32(FIFO_CON(1), FIFOCON_TXEN | FIFOCON_UINC | FIFOCON_TXREQ);

    uint32_t start = millis();
    while (millis() - start < mTxTimeoutMs)
    {
        if (!(mSpi.read32(FIFO_CON(1)) & FIFOCON_TXREQ))
        {
            uint32_t sta = mSpi.read32(FIFO_STA(1));
            if (sta & FIFOSTA_TXERR) return CanTxResult::BusError;
            if (sta & FIFOSTA_TXABT) return CanTxResult::NoAck;
            return CanTxResult::OK;
        }
    }
    return CanTxResult::NoAck;
}

CanError MCP2518Driver::readAndClearErrors()
{
    uint32_t trec = mSpi.read32(REG_CiTREC);
    uint32_t ovif = mSpi.read32(REG_CiRXOVIF);

    CanError e;
    e.rec        = (uint8_t)(trec & 0xFF);
    e.tec        = (uint8_t)((trec >> 8) & 0xFF);
    e.txWarning  = !!(trec & TREC_TXWARN);
    e.rxWarning  = !!(trec & TREC_RXWARN);
    e.txPassive  = !!(trec & TREC_TXBP);
    e.rxPassive  = !!(trec & TREC_RXBP);
    e.busOff     = !!(trec & TREC_TXBO);
    e.rxOverflow = !!(ovif & RXOVIF_FIFO2);
    if (!e.rxOverflow) e.rxOverflow = !!(mSpi.read32(FIFO_STA(2)) & FIFOSTA_RXOVIF);
    if (e.rxOverflow) mSpi.write32(FIFO_STA(2), 0);
    return e;
}

bool MCP2518Driver::hasErrors()
{
    uint32_t trec = mSpi.read32(REG_CiTREC);
    if (trec & (TREC_EWARN | TREC_TXBO)) return true;
    return !!(mSpi.read32(REG_CiRXOVIF) & RXOVIF_FIFO2);
}

bool MCP2518Driver::available()
{
    if (mRxPending) return true;
    return !!(mSpi.read32(FIFO_STA(2)) & FIFOSTA_TFNRFNIF);
}

bool MCP2518Driver::receive(CanMsg& msg, uint32_t timeoutMs)
{
    uint32_t tw = millis();
    while (!available())
    {
        if (millis() - tw >= timeoutMs) return false;
    }

    uint16_t addr = rxRamAddr();

    uint32_t r0 = mSpi.read32(addr);
    uint32_t r1 = mSpi.read32(addr + 4);

    msg.ext = (r1 >> 4) & 1;
    if (msg.ext)
        msg.id = decodeEidT0(r0);
    else
        msg.id = r0 & 0x7FFu;

    msg.fdf = (r1 >> 7) & 1;
    msg.brs = (r1 >> 6) & 1;
    msg.dlc = r1 & 0xFu;

    uint8_t payloadOffset = mTimestamp ? 12 : 8;

    if (mTimestamp)
        msg.timestamp = mSpi.read32(addr + 8);
    else
        msg.timestamp = 0;

    uint8_t len = dlcToLen(msg.dlc);
    for (uint8_t i = 0; i < len; i += 4)
    {
        uint32_t w = mSpi.read32(addr + payloadOffset + i);
        msg.data[i]   =  w        & 0xFF;
        msg.data[i+1] = (w >> 8)  & 0xFF;
        msg.data[i+2] = (w >> 16) & 0xFF;
        msg.data[i+3] = (w >> 24) & 0xFF;
    }

    mSpi.write32(FIFO_CON(2), FIFOCON_UINC);
    mRxPending = false;
    return true;
}

uint8_t MCP2518Driver::getMode()
{
    return mSpi.getMode();
}

CanStatus MCP2518Driver::stop()
{
    mStopPrevMode = mSpi.getMode();
    if (mStopPrevMode == MODE_CONFIG) return CanStatus::OK;
    if (!mSpi.setMode(MODE_CONFIG)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

CanStatus MCP2518Driver::restart()
{
    if (!mSpi.setMode(mStopPrevMode)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

CanStatus MCP2518Driver::sleep()
{
    mSleepPrevMode = mSpi.getMode();
    mSpi.write8(REG_CiCON + 3, MODE_SLEEP << 0);
    uint32_t start = millis();
    while (millis() - start < 100)
    {
        if (mSpi.getMode() == MODE_CONFIG && (mSpi.read32(REG_OSC) & OSC_OSCDIS))
            return CanStatus::OK;
    }
    return CanStatus::MODE_TIMEOUT;
}

CanStatus MCP2518Driver::wake()
{
    uint8_t osc0 = mSpi.read8(REG_OSC);
    mSpi.write8(REG_OSC, osc0 & ~(uint8_t)OSC_OSCDIS);
    uint32_t start = millis();
    while (millis() - start < 10)
    {
        if ((mSpi.read32(REG_OSC) >> 10) & 1) break;  // OSCREADY
    }
    if (!mSpi.setMode(mSleepPrevMode)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

// ----------------------------------------------------------------------------
// Private

uint32_t MCP2518Driver::detectFsys()
{
    uint32_t start = millis();
    while (millis() - start < 10)
    {
        uint32_t osc = mSpi.read32(REG_OSC);
        if ((osc >> 10) & 1)
        {
            bool pllen   = (osc >> 0) & 1;
            bool sclkdiv = (osc >> 4) & 1;
            uint32_t fsys = pllen ? 40000000u : 20000000u;
            if (sclkdiv) fsys /= 2;
            return fsys;
        }
    }
    return 0;
}

void MCP2518Driver::applyTiming(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg)
{
    mSpi.write32(REG_CiNBTCFG, nbtcfg);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);

    uint8_t con2 = mSpi.read8(REG_CiCON + 2);
    con2 &= ~((1u << 4) | (1u << 3));
    con2 |= CON2_RTXAT;
    mSpi.write8(REG_CiCON + 2, con2);
}

void MCP2518Driver::configFifos(uint8_t rxFifoDepth, bool enableTimestamp)
{
    mTimestamp = enableTimestamp;

    uint8_t maxDepth = enableTimestamp ? 23 : 24;
    if (rxFifoDepth < 1)        rxFifoDepth = 1;
    if (rxFifoDepth > maxDepth) rxFifoDepth = maxDepth;

    uint32_t plsize  = (uint32_t)PLSIZE_64 << FIFOCON_PLSIZE_SHIFT;
    uint32_t txFsize = (4u - 1u) << FIFOCON_FSIZE_SHIFT;
    uint32_t rxFsize = (uint32_t)(rxFifoDepth - 1u) << FIFOCON_FSIZE_SHIFT;
    mSpi.write32(FIFO_CON(1), plsize | txFsize | FIFOCON_TXAT_3 | FIFOCON_TXEN);

    uint32_t rxCon = plsize | rxFsize;
    if (enableTimestamp) rxCon |= FIFOCON_RXTSEN;
    if (mIntPin >= 0)    rxCon |= FIFOCON_TFNRFNIE;
    mSpi.write32(FIFO_CON(2), rxCon);

    if (mIntPin >= 0)
    {
        uint8_t intByte2 = mSpi.read8(REG_CiINT + 2);
        mSpi.write8(REG_CiINT + 2, intByte2 | CINT2_RXIE);
    }
}

void MCP2518Driver::configFilter()
{
    setFilter(0, 0, 0, false);
}

void MCP2518Driver::setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext)
{
    if (index > 31) return;

    uint16_t conReg  = FLTCON_REG(index);
    uint8_t  conByte = FLTCON_BYTE(index);
    mSpi.write8(conReg + conByte, 0x00);

    uint32_t obj = ext ? encodeFilterObjEid(id)   : (id   & 0x7FFu);
    uint32_t msk = ext ? encodeFilterMskEid(mask) : (mask & 0x7FFu);

    mSpi.write32(FLTOBJ(index), obj);
    mSpi.write32(FLTMSK(index), msk);

    mSpi.write8(conReg + conByte, (1u << 7) | 0x02);
}

void MCP2518Driver::clearFilter(uint8_t index)
{
    if (index > 31) return;
    mSpi.write8(FLTCON_REG(index) + FLTCON_BYTE(index), 0x00);
}

void MCP2518Driver::resetFilters()
{
    configFilter();  // reinstalls catch-all on filter 0
    for (uint8_t i = 1; i <= 31; i++) clearFilter(i);
}

uint16_t MCP2518Driver::txRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(1)));
}

uint16_t MCP2518Driver::rxRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(2)));
}

// ----------------------------------------------------------------------------
// Static ISR trampoline — no SPI inside ISR, flag only

MCP2518Driver* MCP2518Driver::sIsrInstance = nullptr;

void IRAM_ATTR MCP2518Driver::sIsrHandler()
{
    if (sIsrInstance) sIsrInstance->mRxPending = true;
}
