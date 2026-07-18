#include "mcp2518fd_can.h"
#include "mcp2518fd_registers.h"

MCP2518Driver::MCP2518Driver(SPIClass& spi, uint8_t csPin)
    : mSpi(spi, csPin), mTxTimeoutMs(10), mNbtcfg(0)
{
}

bool MCP2518Driver::configure(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode)
{
    mSpi.begin();
    mSpi.reset();
    delay(20);

    mSpi.setMode(MODE_CONFIG);

    // Bit timing + TDC (config mode only)
    mSpi.write32(REG_CiNBTCFG, nbtcfg);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);

    // Disable TXQ and TEF — not used
    // Enable RTXAT so TXAT field in FIFO CON is respected (DS20006027B page 27)
    uint8_t con2 = mSpi.read8(REG_CiCON + 2);
    con2 &= ~((1u << 4) | (1u << 3));  // clear TXQEN, STEF
    con2 |= CON2_RTXAT;                // set RTXAT — limit retransmissions via TXAT
    mSpi.write8(REG_CiCON + 2, con2);

    configFifos();
    configFilter();
    calcTxTimeout(nbtcfg, dbtcfg);
    mNbtcfg = nbtcfg;
    return mSpi.setMode(mode);
}

bool MCP2518Driver::setDataBitTiming(uint32_t dbtcfg, uint32_t tdcfg)
{
    uint8_t prevMode = mSpi.getMode();  // remember active mode before entering config
    mSpi.setMode(MODE_CONFIG);
    mSpi.write32(REG_CiDBTCFG, dbtcfg);
    mSpi.write32(REG_CiTDC,    tdcfg);
    configFilter();  // re-enable filter after config mode entry resets FIFOs
    calcTxTimeout(mNbtcfg, dbtcfg);
    return mSpi.setMode(prevMode);
}

bool MCP2518Driver::transmit(const CanMsg& msg)
{
    if (!(mSpi.read32(FIFO_STA(1)) & FIFOSTA_TFNRFNIF))
        return false;

    uint16_t addr = txRamAddr();

    // T0: SID[10:0]
    uint32_t t0 = msg.sid & 0x7FFu;
    // T1: FDF, BRS, DLC
    uint32_t t1 = ((msg.fdf ? 1u : 0u) << 7)
                | ((msg.brs ? 1u : 0u) << 6)
                | (msg.dlc & 0xFu);

    mSpi.write32(addr,     t0);
    mSpi.write32(addr + 4, t1);

    // Data — little-endian, 4 bytes per word, up to 64 bytes (16 words)
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

    // Wait for TXREQ to clear — either success or 3 attempts exhausted
    uint32_t start = millis();
    while (millis() - start < mTxTimeoutMs)
    {
        if (!(mSpi.read32(FIFO_CON(1)) & FIFOCON_TXREQ))
        {
            // TXREQ cleared — check if it was aborted (no ACK) or successful
            uint32_t sta = mSpi.read32(FIFO_STA(1));
            return !(sta & (FIFOSTA_TXABT | FIFOSTA_TXERR));
        }
    }
    return false;
}

bool MCP2518Driver::receive(CanMsg& msg)
{
    uint32_t tw = millis();
    while (millis() - tw < 500 && !(mSpi.read32(FIFO_STA(2)) & FIFOSTA_TFNRFNIF)) {}

    if (!(mSpi.read32(FIFO_STA(2)) & FIFOSTA_TFNRFNIF))
        return false;

    uint16_t addr = rxRamAddr();

    uint32_t r0 = mSpi.read32(addr);
    uint32_t r1 = mSpi.read32(addr + 4);

    msg.sid = r0 & 0x7FFu;
    msg.fdf = (r1 >> 7) & 1;
    msg.brs = (r1 >> 6) & 1;
    msg.dlc = r1 & 0xFu;

    // Data — little-endian, 4 bytes per word, up to 64 bytes
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

// --- private ---

void MCP2518Driver::calcTxTimeout(uint32_t nbtcfg, uint32_t dbtcfg)
{
    // Extract fields from register words (DS20006027B Register 3-8/3-9)
    // NBTCFG: BRP[31:24] TSEG1[23:16] TSEG2[14:8]
    // DBTCFG: BRP[31:24] TSEG1[20:16] TSEG2[11:8]
    uint32_t nBrp   = (nbtcfg >> 24) & 0xFF;
    uint32_t nTseg1 = (nbtcfg >> 16) & 0xFF;
    uint32_t nTseg2 = (nbtcfg >>  8) & 0x7F;

    uint32_t dBrp   = (dbtcfg >> 24) & 0xFF;
    uint32_t dTseg1 = (dbtcfg >> 16) & 0x1F;
    uint32_t dTseg2 = (dbtcfg >>  8) & 0x0F;

    // Bit time in ns at 40 MHz (TQ = (BRP+1) * 25 ns)
    uint32_t nbt_ns = (nBrp + 1) * (1 + nTseg1 + nTseg2) * 25;
    uint32_t dbt_ns = (dBrp + 1) * (1 + dTseg1 + dTseg2) * 25;

    // Worst-case 64-byte CAN FD frame (BRS=true, standard ID)
    // Nominal bits: ~36, Data bits: ~562  (see header comment for breakdown)
    uint32_t frame_ns = 36 * nbt_ns + 562 * dbt_ns;

    // 3 retransmission attempts + 2ms margin, rounded up to ms
    uint32_t total_ns = frame_ns * 3;
    mTxTimeoutMs = (total_ns / 1000000) + 2;
    if (mTxTimeoutMs < 2) mTxTimeoutMs = 2;  // floor
}

void MCP2518Driver::configFifos()
{
    uint32_t plsize = (uint32_t)PLSIZE_64 << FIFOCON_PLSIZE_SHIFT;
    uint32_t fsize  = (4u << FIFOCON_FSIZE_SHIFT);  // 5 messages deep (FSIZE=4)
    // FIFO1 = TX: PLSIZE_64, FSIZE=4, TXAT=3 attempts, TXEN
    mSpi.write32(FIFO_CON(1), plsize | fsize | FIFOCON_TXAT_3 | FIFOCON_TXEN);
    // FIFO2 = RX: PLSIZE_64, FSIZE=4
    mSpi.write32(FIFO_CON(2), plsize | fsize);
}

void MCP2518Driver::configFilter()
{
    // Filter 0: accept all, route to FIFO2
    mSpi.write8(REG_CiFLTCON0, 0x00);                 // disable before writing
    mSpi.write32(REG_CiFLTOBJ0, 0x00000000UL);        // match any SID
    mSpi.write32(REG_CiMASK0,   0x00000000UL);        // all bits don't care
    mSpi.write8(REG_CiFLTCON0, (1u << 7) | 0x02);    // FLTEN0=1, F0BP=2
}

uint16_t MCP2518Driver::txRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(1)));
}

uint16_t MCP2518Driver::rxRamAddr()
{
    return (uint16_t)(RAM_BASE + mSpi.read32(FIFO_UA(2)));
}
