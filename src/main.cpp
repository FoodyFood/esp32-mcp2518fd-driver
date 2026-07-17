#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_spi.h"
#include "mcp2518fd_registers.h"

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass spi(VSPI);
MCP2518SPI can(spi, PIN_CS);

static void dumpCiCON()
{
    uint32_t con = can.read32(REG_CiCON);

    Serial.printf("CiCON     = 0x%08lX\n", con);
    Serial.printf("REQOP     = %lu\n",
                  (con & CON_REQOP_MASK) >> CON_REQOP_SHIFT);
    Serial.printf("OPMOD     = %lu\n",
                  (con & CON_OPMOD_MASK) >> CON_OPMOD_SHIFT);
}

void runTest()
{
    Serial.println();
    Serial.println("==========================");

    can.reset();
    delay(20);

    Serial.println("After reset:");
    dumpCiCON();

    can.setMode(MODE_CONFIG);

    Serial.println();
    Serial.println("After CONFIG:");
    dumpCiCON();

    // ------------------------------------------------------------------
    // Program bit timing
    // ------------------------------------------------------------------

    // NBTCFG
    // BRP   = 0
    // TSEG1 = 30
    // TSEG2 = 7
    // SJW   = 7

    uint32_t nbtcfg =
          (0UL  << 24)
        | (30UL << 16)
        | (7UL  << 8)
        | (7UL);

    // DBTCFG
    // BRP   = 0
    // TSEG1 = 14
    // TSEG2 = 3
    // SJW   = 3

    uint32_t dbtcfg =
          (0UL  << 24)
        | (14UL << 16)
        | (3UL  << 8)
        | (3UL);

    can.write32(REG_CiNBTCFG, nbtcfg);
    can.write32(REG_CiDBTCFG, dbtcfg);

    Serial.println();
    Serial.printf("CiNBTCFG  = 0x%08lX\n", can.read32(REG_CiNBTCFG));
    Serial.printf("CiDBTCFG  = 0x%08lX\n", can.read32(REG_CiDBTCFG));

    // ------------------------------------------------------------------
    // Step 2: Configure FIFO1 (TX) and FIFO2 (RX)
    // Must be done in Configuration mode.
    // Clear TXQEN (bit 20) and STEF (bit 19) in CiCON byte 2 — we don't use TXQ or TEF.
    // FIFO1 = TX: TXEN=1, PLSIZE=0 (8 bytes), FSIZE=0 (1 deep)
    // FIFO2 = RX: TXEN=0, PLSIZE=0, FSIZE=0
    // Expected readback: CiFIFOCON1=0x00000080, CiFIFOCON2=0x00000000
    // ------------------------------------------------------------------

    // Clear TXQEN and STEF in CiCON byte 2 (bits 4 and 3 of that byte)
    uint8_t con2 = can.read8(REG_CiCON + 2);
    con2 &= ~((1u << 4) | (1u << 3));  // clear TXQEN (bit20=byte2.bit4), STEF (bit19=byte2.bit3)
    can.write8(REG_CiCON + 2, con2);

    // FIFO1 = TX: TXEN=1, PLSIZE=0, FSIZE=0
    can.write32(FIFO_CON(1), FIFOCON_TXEN);

    // FIFO2 = RX: all zeros
    can.write32(FIFO_CON(2), 0x00000000);

    uint32_t f1con = can.read32(FIFO_CON(1));
    uint32_t f2con = can.read32(FIFO_CON(2));
    uint32_t cicon = can.read32(REG_CiCON);

    Serial.println();
    Serial.println("Step 2 — FIFO config:");
    Serial.printf("CiCON     = 0x%08lX  TXQEN=%lu STEF=%lu\n",
        cicon,
        (cicon >> 20) & 1,
        (cicon >> 19) & 1);
    // FRESET (bit10) is set automatically by hardware in config mode — expected
    Serial.printf("CiFIFOCON1= 0x%08lX  expect 0x00000480  %s\n",
        f1con, f1con == 0x00000480UL ? "OK" : "FAIL");
    Serial.printf("CiFIFOCON2= 0x%08lX  expect 0x00000400  %s\n",
        f2con, f2con == 0x00000400UL ? "OK" : "FAIL");

    // ------------------------------------------------------------------
    // Step 3: Read CiFIFOUA1 and CiFIFOUA2 in loopback mode (outside config)
    // TEF=0, TXQ=0 => FIFO1 starts at RAM base 0x400
    // Message object size = 8 bytes header + 8 bytes payload (PLSIZE=0) = 16 bytes
    // Expected: UA1=0x400, UA2=0x410
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // Step 5: Configure Filter 0 BEFORE entering loopback, then TX+RX
    // Filter must be configured before the frame is transmitted.
    // Sequence:
    //   1. In config mode: disable filter, write OBJ+MASK, enable pointing to FIFO2
    //   2. Enter loopback
    //   3. Transmit frame (Step 4)
    //   4. Check FIFO2 has message, read and verify
    // ------------------------------------------------------------------

    // Configure Filter 0 (can be done outside config mode per ref manual 6.1,
    // but we are still in config mode here so this is fine)
    can.write8(REG_CiFLTCON0, 0x00);                  // disable filter 0
    can.write32(REG_CiFLTOBJ0, 0x00000000UL);          // match any SID
    can.write32(REG_CiMASK0,   0x00000000UL);          // all bits don’t care
    can.write8(REG_CiFLTCON0, (1u << 7) | 0x02);      // FLTEN0=1, F0BP=2 (FIFO2)

    // Now enter loopback
    can.setMode(MODE_INTERNAL_LB);

    uint32_t ua1 = can.read32(FIFO_UA(1));
    uint32_t ua2 = can.read32(FIFO_UA(2));

    Serial.println();
    Serial.println("Step 3 — FIFO UA (RAM addresses):");
    Serial.printf("CiFIFOUA1 = 0x%08lX  expect 0x00000000  %s\n",
        ua1, ua1 == 0x00000000UL ? "OK" : "FAIL");
    Serial.printf("CiFIFOUA2 = 0x%08lX  expect 0x00000010  %s\n",
        ua2, ua2 == 0x00000010UL ? "OK" : "FAIL");
    Serial.printf("RAM addr FIFO1 = 0x%03lX  RAM addr FIFO2 = 0x%03lX\n",
        0x400UL + ua1, 0x400UL + ua2);

    // ------------------------------------------------------------------
    // Step 4: Transmit one CAN FD frame
    // ------------------------------------------------------------------

    Serial.println();
    Serial.println("Step 4 — TX frame:");

    uint32_t sta1 = can.read32(FIFO_STA(1));
    Serial.printf("CiFIFOSTA1 before TX = 0x%08lX  TFNRFNIF=%lu\n",
        sta1, (sta1 & FIFOSTA_TFNRFNIF) ? 1 : 0);
    Serial.printf("TFNRFNIF check: %s\n",
        (sta1 & FIFOSTA_TFNRFNIF) ? "OK" : "FAIL");

    uint16_t txAddr = (uint16_t)(RAM_BASE + ua1);
    can.write32(txAddr,      0x00000123UL);
    can.write32(txAddr + 4,  0x000000C8UL);
    can.write32(txAddr + 8,  0x04030201UL);
    can.write32(txAddr + 12, 0x08070605UL);

    can.write32(FIFO_CON(1), FIFOCON_TXEN | FIFOCON_UINC | FIFOCON_TXREQ);

    uint32_t start = millis();
    bool sent = false;
    while (millis() - start < 10)
    {
        if (!(can.read32(FIFO_CON(1)) & FIFOCON_TXREQ))
        {
            sent = true;
            break;
        }
    }
    Serial.printf("TXREQ cleared: %s\n", sent ? "OK" : "FAIL");

    uint32_t sta1_after = can.read32(FIFO_STA(1));
    Serial.printf("CiFIFOSTA1 after TX  = 0x%08lX\n", sta1_after);
    Serial.printf("No TX errors: %s\n",
        (sta1_after & (FIFOSTA_TXERR | FIFOSTA_TXABT)) == 0 ? "OK" : "FAIL");

    // ------------------------------------------------------------------
    // Step 5: Receive the frame from FIFO2
    // ------------------------------------------------------------------

    Serial.println();
    Serial.println("Step 5 — receive frame:");

    uint32_t sta2 = can.read32(FIFO_STA(2));
    Serial.printf("CiFIFOSTA2 = 0x%08lX  TFNRFNIF=%lu\n",
        sta2, (sta2 & FIFOSTA_TFNRFNIF) ? 1 : 0);
    Serial.printf("Message waiting: %s\n",
        (sta2 & FIFOSTA_TFNRFNIF) ? "OK" : "FAIL");

    uint32_t ua2_rx = can.read32(FIFO_UA(2));
    uint16_t rxAddr = (uint16_t)(RAM_BASE + ua2_rx);

    uint32_t r0 = can.read32(rxAddr);
    uint32_t r1 = can.read32(rxAddr + 4);
    uint32_t r2 = can.read32(rxAddr + 8);
    uint32_t r3 = can.read32(rxAddr + 12);

    Serial.printf("R0 = 0x%08lX  SID=0x%03lX  expect SID=0x123  %s\n",
        r0, r0 & 0x7FFUL, (r0 & 0x7FFUL) == 0x123UL ? "OK" : "FAIL");
    Serial.printf("R1 = 0x%08lX  FDF=%lu BRS=%lu DLC=%lu  expect FDF=1 BRS=1 DLC=8  %s\n",
        r1,
        (r1 >> 7) & 1,
        (r1 >> 6) & 1,
        r1 & 0xFUL,
        ((r1 & 0xCFUL) == 0xC8UL) ? "OK" : "FAIL");
    Serial.printf("R2 = 0x%08lX  expect 0x04030201  %s\n",
        r2, r2 == 0x04030201UL ? "OK" : "FAIL");
    Serial.printf("R3 = 0x%08lX  expect 0x08070605  %s\n",
        r3, r3 == 0x08070605UL ? "OK" : "FAIL");

    can.write32(FIFO_CON(2), FIFOCON_UINC);

    Serial.println();
}

void setup()
{
    Serial.begin(115200);

    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    can.begin();

    Serial.println("Press any key...");
}

void loop()
{
    while (Serial.available())
    {
        Serial.read();
        runTest();
        Serial.println("Press any key...");
    }
}