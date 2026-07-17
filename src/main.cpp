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
    // Step 3: Read CiFIFOUA1 and CiFIFOUA2 — must be outside config mode
    // Enter loopback first, then read UA
    // ------------------------------------------------------------------

    can.setMode(MODE_INTERNAL_LB);

    uint32_t ua1 = can.read32(FIFO_UA(1));
    uint32_t ua2 = can.read32(FIFO_UA(2));

    Serial.println();
    Serial.println("Step 3 — FIFO UA (RAM addresses):");
    // UA holds the offset from RAM base (0x400), not the absolute address
    // Actual address = 0x400 + UA. With no TEF/TXQ:
    // FIFO1: offset 0x000, FIFO2: offset 0x010 (16 bytes per object)
    Serial.printf("CiFIFOUA1 = 0x%08lX  expect 0x00000000  %s\n",
        ua1, ua1 == 0x00000000UL ? "OK" : "FAIL");
    Serial.printf("CiFIFOUA2 = 0x%08lX  expect 0x00000010  %s\n",
        ua2, ua2 == 0x00000010UL ? "OK" : "FAIL");
    Serial.printf("RAM addr FIFO1 = 0x%03lX  RAM addr FIFO2 = 0x%03lX\n",
        0x400UL + ua1, 0x400UL + ua2);

    // ------------------------------------------------------------------
    // Step 4: Transmit one CAN FD frame in internal loopback
    // Sequence (ref manual section 4.2):
    //   1. Check TFNRFNIF in CiFIFOSTA1 (must be set = room in FIFO)
    //   2. Write T0+T1+data to RAM at 0x400 + UA1
    //   3. Set UINC|TXREQ in one write to CiFIFOCON1
    //   4. Poll TXREQ cleared = frame sent
    //   5. Verify no error flags in CiFIFOSTA1
    //
    // T0 = SID=0x123, IDE=0  => 0x00000123
    // T1 = FDF=1, BRS=1, DLC=8 => 0x000000C8
    // Data: 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
    // ------------------------------------------------------------------

    Serial.println();
    Serial.println("Step 4 — TX frame:");

    uint32_t sta1 = can.read32(FIFO_STA(1));
    Serial.printf("CiFIFOSTA1 before TX = 0x%08lX  TFNRFNIF=%lu\n",
        sta1, (sta1 & FIFOSTA_TFNRFNIF) ? 1 : 0);
    Serial.printf("TFNRFNIF check: %s\n",
        (sta1 & FIFOSTA_TFNRFNIF) ? "OK" : "FAIL");

    // Write message object to RAM
    uint16_t txAddr = (uint16_t)(RAM_BASE + ua1);
    can.write32(txAddr,      0x00000123UL);  // T0: SID=0x123
    can.write32(txAddr + 4,  0x000000C8UL);  // T1: FDF=1 BRS=1 DLC=8
    can.write32(txAddr + 8,  0x04030201UL);  // T2: bytes 0-3
    can.write32(txAddr + 12, 0x08070605UL);  // T3: bytes 4-7

    // Set UINC and TXREQ in one write — rule: always set together
    can.write32(FIFO_CON(1), FIFOCON_TXEN | FIFOCON_UINC | FIFOCON_TXREQ);

    // Poll TXREQ cleared (max 10ms)
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