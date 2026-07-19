#include "mcp2518fd_can.h"
#include "mcp2518fd_registers.h"

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
    calcTxTimeout(nbtcfg, dbtcfg);
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
    calcTxTimeout(mNbtcfg, dbtcfg);

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
    calcTxTimeout(nbtcfg, dbtcfg);
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
    calcTxTimeout(mNbtcfg, dbtcfg);
    if (!mSpi.setMode(prevMode)) return CanStatus::MODE_TIMEOUT;
    return CanStatus::OK;
}

bool MCP2518Driver::transmit(const CanMsg& msg)
{
    if (!(mSpi.read32(FIFO_STA(1)) & FIFOSTA_TFNRFNIF))
        return false;

    uint16_t addr = txRamAddr();

    uint32_t t0 = msg.id & 0x7FFu;
    uint32_t t1 = ((msg.fdf ? 1u : 0u) << 7)
                | ((msg.brs ? 1u : 0u) << 6)
                | (msg.dlc & 0xFu);

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

    msg.id  = r0 & 0x7FFu;
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
// Private

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

bool MCP2518Driver::calcBitTiming(uint32_t fsys, uint32_t nominalBps, uint32_t dataBps,
                                   uint32_t& nbtcfg, uint32_t& dbtcfg, uint32_t& tdcfg)
{
    // Nominal: BRP=0, maximise TSEG1 at 80% SP
    // rate = fsys / ((BRP+1) * (1 + TSEG1 + TSEG2))
    // With BRP=0, SP=80%: TSEG2 = totalTQ / 5, TSEG1 = totalTQ - 1 - TSEG2
    // TSEG1 field: 8-bit (max 255), TSEG2 field: 7-bit (max 127)
    {
        uint32_t totalTQ = fsys / nominalBps;
        if (totalTQ < 3 || (fsys % nominalBps) != 0) return false;

        uint32_t tseg2 = totalTQ / 5;          // 20% for phase seg 2
        if (tseg2 < 1) tseg2 = 1;
        if (tseg2 > 127) tseg2 = 127;
        uint32_t tseg1 = totalTQ - 1 - tseg2;  // remaining TQs
        if (tseg1 > 255) return false;
        if (tseg1 < 1)   return false;

        uint32_t sjw = tseg2;  // SJW = TSEG2 is standard practice
        nbtcfg = (0u << 24) | (tseg1 << 16) | (tseg2 << 8) | sjw;
    }

    // Data: BRP=0, maximise TSEG1 at 80% SP
    // TSEG1 field: 5-bit (max 31), TSEG2 field: 4-bit (max 15)
    {
        uint32_t totalTQ = fsys / dataBps;
        if (totalTQ < 3 || (fsys % dataBps) != 0) return false;

        uint32_t tseg2 = totalTQ / 5;
        if (tseg2 < 1) tseg2 = 1;
        if (tseg2 > 15) tseg2 = 15;
        uint32_t tseg1 = totalTQ - 1 - tseg2;
        if (tseg1 > 31) tseg1 = 31;  // cap to field width, recheck rate below
        if (tseg1 < 1)  return false;

        // Verify the capped values still produce the exact target rate
        if (fsys / (1 + tseg1 + tseg2) != dataBps) return false;

        uint32_t sjw = tseg2;
        dbtcfg = (0u << 24) | (tseg1 << 16) | (tseg2 << 8) | sjw;

        // TDC: required at >= 1 Mbps. TDCO = (BRP+1)*(TSEG1+1) [ref manual Table 3-5]
        if (dataBps >= 1000000)
        {
            uint32_t tdco = (tseg1 + 1);  // BRP=0 so (BRP+1)=1
            tdcfg = (2u << 16) | (tdco << 8);  // TDCMOD=auto(2), TDCO
        }
        else
        {
            tdcfg = 0;  // TDC disabled
        }
    }

    return true;
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

void MCP2518Driver::calcTxTimeout(uint32_t nbtcfg, uint32_t dbtcfg)
{
    uint32_t nBrp   = (nbtcfg >> 24) & 0xFF;
    uint32_t nTseg1 = (nbtcfg >> 16) & 0xFF;
    uint32_t nTseg2 = (nbtcfg >>  8) & 0x7F;
    uint32_t dBrp   = (dbtcfg >> 24) & 0xFF;
    uint32_t dTseg1 = (dbtcfg >> 16) & 0x1F;
    uint32_t dTseg2 = (dbtcfg >>  8) & 0x0F;

    // TQ in ns = (BRP+1) * (1e9 / FSYS). Use mFsys if available, else assume 20 MHz.
    uint32_t fsys = mFsys ? mFsys : 20000000u;
    uint32_t tq_ns = 1000000000u / fsys;

    uint32_t nbt_ns = (nBrp + 1) * (1 + nTseg1 + nTseg2) * tq_ns;
    uint32_t dbt_ns = (dBrp + 1) * (1 + dTseg1 + dTseg2) * tq_ns;

    // Worst-case 64-byte CAN FD frame: ~36 nominal bits + ~562 data bits
    uint32_t frame_ns = 36 * nbt_ns + 562 * dbt_ns;
    mTxTimeoutMs = (frame_ns * 3 / 1000000) + 2;
    if (mTxTimeoutMs < 2) mTxTimeoutMs = 2;
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
    mSpi.write8(REG_CiFLTCON0, 0x00);
    mSpi.write32(REG_CiFLTOBJ0, 0x00000000UL);
    mSpi.write32(REG_CiMASK0,   0x00000000UL);
    mSpi.write8(REG_CiFLTCON0, (1u << 7) | 0x02);
}

uint16_t MCP2518Driver::txRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(1)));
}

uint16_t MCP2518Driver::rxRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(2)));
}
