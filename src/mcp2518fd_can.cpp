#include "mcp2518fd_can.h"
#include "mcp2518fd_registers.h"
#include "mcp2518fd_timing.h"

MCP2518Driver::MCP2518Driver(SPIClass& spi, uint8_t csPin, int8_t intPin)
    : mSpi(spi, csPin), mFsys(0), mTxTimeoutMs(10), mNbtcfg(0), mIntPin(intPin), mRxPending(false)
{
}

// ----------------------------------------------------------------------------
// Public API

CanStatus MCP2518Driver::configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode,
                                   uint8_t rxFifoDepth, bool enableTimestamp)
{
    mSpi.begin();
    mSpi.reset();
    delay(20);
    mSpi.setMode(MODE_CONFIG);

    mFsys = detectFsys();
    if (mFsys == 0) return CanStatus::CLOCK_NOT_READY;

    uint32_t nbtcfg, dbtcfg, tdcfg;
    if (!calcBitTiming(mFsys, nominalBps, dataBps, nbtcfg, dbtcfg, tdcfg))
        return CanStatus::RATE_NOT_ACHIEVABLE;

    applyTiming(nbtcfg, dbtcfg, tdcfg);
    configFifos(rxFifoDepth, enableTimestamp);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(mFsys, nbtcfg, dbtcfg);
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;

    // Enable TBC after mode transition — CiTSCON is not config-mode-only
    // and must be set after exiting config mode to survive the transition
    if (enableTimestamp)
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

CanStatus MCP2518Driver::configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode,
                                      uint8_t rxFifoDepth, bool enableTimestamp)
{
    mSpi.begin();
    mSpi.reset();
    delay(20);
    mSpi.setMode(MODE_CONFIG);

    applyTiming(nbtcfg, dbtcfg, tdcfg);
    configFifos(rxFifoDepth, enableTimestamp);
    configFilter();
    mTxTimeoutMs = calcTxTimeout(0, nbtcfg, dbtcfg);  // fsys unknown in raw path
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;

    if (enableTimestamp)
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

CanError MCP2518Driver::getErrors()
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
    // Also check per-FIFO RXOVIF in CiFIFOSTA2 (bit 3) — set when FIFO is full and a frame is discarded
    // CiRXOVIF is the aggregate; CiFIFOSTA2.RXOVIF is the per-FIFO source
    if (!e.rxOverflow) e.rxOverflow = !!(mSpi.read32(FIFO_STA(2)) & FIFOSTA_RXOVIF);
    // Clear: write 0 to CiFIFOSTA2 to clear RXOVIF bit (DS20005678E page 62)
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

bool MCP2518Driver::receive(CanMsg& msg)
{
    if (!available()) return false;
    return receive(msg, 0);
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
    {
        // R0[10:0]=SID[10:0], R0[28:11]=EID[17:0]  (DS20006027B Table 3-6)
        msg.id = decodeEidT0(r0);
    }
    else
    {
        msg.id = r0 & 0x7FFu;
    }
    msg.fdf = (r1 >> 7) & 1;
    msg.brs = (r1 >> 6) & 1;
    msg.dlc = r1 & 0xFu;

    // RX message object layout (DS20006027B Table 3-6 / ref manual Table 7-1):
    //   R0 (+0)  — SID/EID
    //   R1 (+4)  — FILHIT, ESI, FDF, BRS, RTR, IDE, DLC
    //   R2 (+8)  — RXMSGTS (only present when RXTSEN=1)
    //   R3 (+12 with timestamp, +8 without) — payload bytes 0-3
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

uint32_t MCP2518Driver::readOsc()
{
    return mSpi.read32(REG_OSC);
}

// ----------------------------------------------------------------------------
// Private — timing helpers are free functions in mcp2518fd_timing.h

uint32_t MCP2518Driver::detectFsys()
{
    // OSC register (0xE00), byte 0 layout (DS20006027B Register 3-1):
    //   bit 0 = PLLEN   — 1: PLL enabled (requires 4 MHz input, x10 = 40 MHz)
    //   bit 4 = SCLKDIV — 1: SYSCLK divided by 2
    // OSC byte 1:
    //   bit 2 (bit 10 overall) = OSCREADY
    //
    // Wait for OSCREADY before reading SCLKDIV/PLLEN
    uint32_t start = millis();
    while (millis() - start < 10)
    {
        uint32_t osc = mSpi.read32(REG_OSC);
        if ((osc >> 10) & 1)  // OSCREADY
        {
            bool pllen   = (osc >> 0) & 1;
            bool sclkdiv = (osc >> 4) & 1;
            uint32_t fsys = pllen ? 40000000u : 20000000u;  // PLL: 4MHz*10=40MHz; no PLL: crystal direct
            if (sclkdiv) fsys /= 2;
            return fsys;
        }
    }
    return 0;  // clock not ready
}

void MCP2518Driver::applyTiming(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg)
{
    mSpi.write32(REG_CiNBTCFG, nbtcfg);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);

    // Disable TXQ and TEF, enable RTXAT
    uint8_t con2 = mSpi.read8(REG_CiCON + 2);
    con2 &= ~((1u << 4) | (1u << 3));
    con2 |= CON2_RTXAT;
    mSpi.write8(REG_CiCON + 2, con2);
}

void MCP2518Driver::configFifos(uint8_t rxFifoDepth, bool enableTimestamp)
{
    mTimestamp = enableTimestamp;

    // RAM budget: FIFO1 (TX, depth=4) = 4 × 72 = 288 bytes; remaining = 1760 bytes
    // Without timestamp: slot = R0+R1+payload = 4+4+64 = 72 bytes → floor(1760/72) = 24
    // With timestamp:    slot = R0+R1+R2+payload = 4+4+4+64 = 76 bytes → floor(1760/76) = 23
    uint8_t maxDepth = enableTimestamp ? 23 : 24;
    if (rxFifoDepth < 1)        rxFifoDepth = 1;
    if (rxFifoDepth > maxDepth) rxFifoDepth = maxDepth;

    uint32_t plsize   = (uint32_t)PLSIZE_64 << FIFOCON_PLSIZE_SHIFT;
    uint32_t txFsize  = (4u - 1u) << FIFOCON_FSIZE_SHIFT;  // FSIZE field = depth-1
    uint32_t rxFsize  = (uint32_t)(rxFifoDepth - 1u) << FIFOCON_FSIZE_SHIFT;
    mSpi.write32(FIFO_CON(1), plsize | txFsize | FIFOCON_TXAT_3 | FIFOCON_TXEN);

    // RXTSEN (bit 5) enables per-message timestamp capture in RAM (DS20006027B page 55)
    // TFNRFNIE (bit 0) enables the per-FIFO not-empty interrupt — feeds CiINT.RXIF
    uint32_t rxCon = plsize | rxFsize;
    if (enableTimestamp) rxCon |= FIFOCON_RXTSEN;
    if (mIntPin >= 0)    rxCon |= FIFOCON_TFNRFNIE;
    mSpi.write32(FIFO_CON(2), rxCon);

    // Enable RXIE in CiINT so the INT pin asserts on RX (DS20006027B Register 3-14 bit 17)
    if (mIntPin >= 0)
    {
        uint8_t intByte2 = mSpi.read8(REG_CiINT + 2);
        mSpi.write8(REG_CiINT + 2, intByte2 | CINT2_RXIE);
    }
}

void MCP2518Driver::configFilter()
{
    // Catch-all: id=0, mask=0 (all don't-care), ext=false (MIDE=0 → match both SID and EID)
    setFilter(0, 0, 0, false);
}

void MCP2518Driver::setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext)
{
    if (index > 31) return;

    // Disable filter before modifying OBJ/MASK (DS20006027B Register 3-32 Note 1)
    uint16_t conReg  = FLTCON_REG(index);
    uint8_t  conByte = FLTCON_BYTE(index);
    mSpi.write8(conReg + conByte, 0x00);

    // Encode OBJ: SID[10:0] at bits[10:0], EID[17:0] at bits[28:11], EXIDE at bit[30]
    // Same bit layout as T0/R0 message object (DS20006027B Register 3-33)
    uint32_t obj = ext ? encodeFilterObjEid(id)   : (id   & 0x7FFu);
    uint32_t msk = ext ? encodeFilterMskEid(mask) : (mask & 0x7FFu);

    mSpi.write32(FLTOBJ(index), obj);
    mSpi.write32(FLTMSK(index), msk);

    // Re-enable: FLTEN=1, route to FIFO2 (DS20006027B Register 3-32)
    mSpi.write8(conReg + conByte, (1u << 7) | 0x02);
}

void MCP2518Driver::clearFilter(uint8_t index)
{
    if (index > 31) return;
    mSpi.write8(FLTCON_REG(index) + FLTCON_BYTE(index), 0x00);
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
