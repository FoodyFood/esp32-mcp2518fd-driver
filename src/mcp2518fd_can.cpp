#include "mcp2518fd_can.h"
#include "mcp2518fd_registers.h"
#include "mcp2518fd_timing.h"

MCP2518Driver::MCP2518Driver(SPIClass& spi, uint8_t csPin)
    : mSpi(spi, csPin), mFsys(0), mTxTimeoutMs(10), mNbtcfg(0)
{
}

// ----------------------------------------------------------------------------
// Public API

CanStatus MCP2518Driver::configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode)
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
    configFifos();
    configFilter();
    mTxTimeoutMs = calcTxTimeout(mFsys, nbtcfg, dbtcfg);
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;
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

CanStatus MCP2518Driver::configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode)
{
    mSpi.begin();
    mSpi.reset();
    delay(20);
    mSpi.setMode(MODE_CONFIG);

    applyTiming(nbtcfg, dbtcfg, tdcfg);
    configFifos();
    configFilter();
    mTxTimeoutMs = calcTxTimeout(0, nbtcfg, dbtcfg);  // fsys unknown in raw path
    mNbtcfg = nbtcfg;

    if (!mSpi.setMode(mode)) return CanStatus::MODE_TIMEOUT;
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

bool MCP2518Driver::transmit(const CanMsg& msg)
{
    if (!(mSpi.read32(FIFO_STA(1)) & FIFOSTA_TFNRFNIF))
        return false;

    uint16_t addr = txRamAddr();

    uint32_t t0, t1;
    if (msg.ext)
    {
        // 29-bit EID: T0[10:0]=SID[10:0]=id>>18, T0[28:11]=EID[17:0]=id&0x3FFFF
        // T1 bit4=IDE=1  (DS20006027B Table 3-5)
        t0 = encodeEidT0(msg.id);
        t1 = (1u << 4)  // IDE
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
            return !(sta & (FIFOSTA_TXABT | FIFOSTA_TXERR));
        }
    }
    return false;
}

bool MCP2518Driver::available()
{
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

    uint8_t len = dlcToLen(msg.dlc);
    for (uint8_t i = 0; i < len; i += 4)
    {
        uint32_t w = mSpi.read32(addr + 8 + i);
        msg.data[i]   =  w        & 0xFF;
        msg.data[i+1] = (w >> 8)  & 0xFF;
        msg.data[i+2] = (w >> 16) & 0xFF;
        msg.data[i+3] = (w >> 24) & 0xFF;
    }

    mSpi.write32(FIFO_CON(2), FIFOCON_UINC);
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

void MCP2518Driver::configFifos()
{
    uint32_t plsize = (uint32_t)PLSIZE_64 << FIFOCON_PLSIZE_SHIFT;
    uint32_t fsize  = (4u << FIFOCON_FSIZE_SHIFT);
    mSpi.write32(FIFO_CON(1), plsize | fsize | FIFOCON_TXAT_3 | FIFOCON_TXEN);
    mSpi.write32(FIFO_CON(2), plsize | fsize);
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
