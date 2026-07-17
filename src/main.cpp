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

    // Now enter loopback
    can.setMode(MODE_INTERNAL_LB);

    Serial.println();
    Serial.println("After LOOPBACK:");
    dumpCiCON();

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