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

    can.setMode(MODE_INTERNAL_LB);

    Serial.println();
    Serial.println("After LOOPBACK:");
    dumpCiCON();

    // ------------------------------------------------------------------
    // Verify FIFO register addresses (Step 1 sanity check)
    // In config mode, FRESET is set in all FIFOCONm registers (bit 10)
    // TXEN is clear (RX FIFO by default)
    // All other bits should be reset values
    // ------------------------------------------------------------------

    Serial.println();
    Serial.println("FIFO register address check (in loopback/config):");

    uint32_t txqcon  = can.read32(REG_CiTXQCON);
    uint32_t f1con   = can.read32(REG_CiFIFOCON1);
    uint32_t f1sta   = can.read32(REG_CiFIFOSTA1);
    uint32_t f2con   = can.read32(REG_CiFIFOCON2);

    Serial.printf("CiTXQCON  (0x050) = 0x%08lX  FRESET=%lu TXEN=%lu\n",
        txqcon,
        (txqcon & FIFOCON_FRESET) ? 1 : 0,
        (txqcon & FIFOCON_TXEN)   ? 1 : 0);

    Serial.printf("CiFIFOCON1(0x05C) = 0x%08lX  FRESET=%lu TXEN=%lu\n",
        f1con,
        (f1con & FIFOCON_FRESET) ? 1 : 0,
        (f1con & FIFOCON_TXEN)   ? 1 : 0);

    Serial.printf("CiFIFOSTA1(0x060) = 0x%08lX  TFNRFNIF=%lu\n",
        f1sta,
        (f1sta & FIFOSTA_TFNRFNIF) ? 1 : 0);

    Serial.printf("CiFIFOCON2(0x068) = 0x%08lX  FRESET=%lu TXEN=%lu\n",
        f2con,
        (f2con & FIFOCON_FRESET) ? 1 : 0,
        (f2con & FIFOCON_TXEN)   ? 1 : 0);

    // Also verify FIFO_CON() helper gives same result as direct address
    uint32_t f1con_helper = can.read32(FIFO_CON(1));
    uint32_t f2con_helper = can.read32(FIFO_CON(2));
    Serial.printf("FIFO_CON(1) helper match: %s\n", f1con_helper == f1con ? "OK" : "FAIL");
    Serial.printf("FIFO_CON(2) helper match: %s\n", f2con_helper == f2con ? "OK" : "FAIL");

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