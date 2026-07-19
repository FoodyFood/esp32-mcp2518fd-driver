#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Scope loopback — single board, continuous TX for oscilloscope measurements
//
// Uses MODE_EXTERNAL_LB: drives real signals on CANH/CANL via the ATA6561
// transceiver while the chip ACKs its own frames internally.
// No second node required.
//
// Press any key to cycle through data rates:
//   125 kbps nominal / 2 Mbps data  (default)
//   125 kbps nominal / 4 Mbps data
//   125 kbps nominal / 5 Mbps data
//   125 kbps nominal / 8 Mbps data
//
// Frame: SID=0x123, DLC=8, data=0x01..0x08, every 10ms.
// Serial monitor shows current rate and frame count.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

constexpr uint32_t FRAME_INTERVAL_MS = 10;
constexpr uint32_t STATUS_EVERY      = 100;  // print every N frames

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

struct Rate { uint32_t bps; const char* label; };
constexpr Rate RATES[] = {
    { 2000000, "125kbps / 2 Mbps" },
    { 4000000, "125kbps / 4 Mbps" },
    { 5000000, "125kbps / 5 Mbps" },
};
constexpr int NUM_RATES = sizeof(RATES) / sizeof(RATES[0]);

static int      rateIdx    = 0;
static uint32_t frameCount = 0;

static void applyRate(int idx)
{
    can.setDataRate(RATES[idx].bps);
    frameCount = 0;
    Serial.printf("\nRate: %s  (press any key to cycle)\n", RATES[idx].label);
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(125000, 2000000, MODE_EXTERNAL_LB) == CanStatus::OK;

    Serial.println();
    Serial.println("==========================");
    Serial.println("  CAN FD Scope Loopback");
    Serial.println("==========================");
    Serial.printf("configure(MODE_EXTERNAL_LB): %s\n", ok ? "OK" : "FAIL");
    Serial.printf("FSYS detected: %lu Hz\n", can.getFsys());
    Serial.printf("OSC reg: 0x%08X\n", can.readOsc());
    Serial.printf("SID=0x123  DLC=8  data=0x01..0x08  interval=%dms\n", FRAME_INTERVAL_MS);
    Serial.printf("\nRate: %s  (press any key to cycle)\n", RATES[rateIdx].label);
}

void loop()
{
    if (Serial.available())
    {
        Serial.read();
        rateIdx = (rateIdx + 1) % NUM_RATES;
        applyRate(rateIdx);
    }

    CanMsg tx;
    tx.id = 0x123; tx.fdf = true; tx.brs = true; tx.dlc = 8;
    for (int i = 0; i < 8; i++) tx.data[i] = 0x01 + i;

    if (!can.transmit(tx))
        Serial.println("[TX failed]");
    else
        frameCount++;

    if (frameCount % STATUS_EVERY == 0)
        Serial.printf("  %s  frames=%lu\n", RATES[rateIdx].label, frameCount);

    delay(FRAME_INTERVAL_MS);
}
