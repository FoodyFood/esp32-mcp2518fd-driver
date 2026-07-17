#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Two-node CAN FD — Step 1: continuous TX in MODE_EXTERNAL_LB
//
// Drives real signals on TXCAN/RXCAN (scope-visible) while the chip
// ACKs its own frames, so no second node is needed yet.
//
// Frame: SID=0x123, FDF=true, BRS=true, DLC=8, data=0x01..0x08
// Rate:  125 kbps nominal / 2 Mbps data
//
// Press any key to toggle transmit on/off.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

constexpr uint32_t TX_INTERVAL_MS  = 10;   // ms between frames — easy scope trigger
constexpr uint32_t STATUS_EVERY    = 100;  // print status every N frames

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static bool     running   = false;
static uint32_t frameCount = 0;

static CanMsg makeTxFrame()
{
    CanMsg tx;
    tx.sid = 0x123;
    tx.fdf = true;
    tx.brs = true;
    tx.dlc = 8;
    for (int i = 0; i < 8; i++) tx.data[i] = 0x01 + i;
    return tx;
}

static void printStatus()
{
    Serial.printf("  mode=EXTERNAL_LB  125kbps/2Mbps  SID=0x123  DLC=8  frames=%lu\n",
                  frameCount);
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(
        NBTCFG_125K_40MHZ,
        DBTCFG_2M_40MHZ,
        TDC_2M_40MHZ,
        MODE_EXTERNAL_LB);

    Serial.println();
    Serial.println("==========================");
    Serial.printf("configure(MODE_EXTERNAL_LB): %s\n", ok ? "OK" : "FAIL");
    Serial.printf("mode confirmed: %d (expected %d)\n", can.getMode(), MODE_EXTERNAL_LB);
    Serial.println("Press any key to start transmitting...");
}

void loop()
{
    // Toggle on keypress
    if (Serial.available())
    {
        Serial.read();
        running = !running;
        frameCount = 0;
        Serial.println(running ? "TX started." : "TX stopped.");
        if (running) printStatus();
    }

    if (!running) return;

    CanMsg tx = makeTxFrame();
    bool ok = can.transmit(tx);

    if (!ok)
    {
        Serial.println("  transmit() FAILED — stopping.");
        running = false;
        return;
    }

    frameCount++;

    if (frameCount % STATUS_EVERY == 0)
        printStatus();

    delay(TX_INTERVAL_MS);
}
