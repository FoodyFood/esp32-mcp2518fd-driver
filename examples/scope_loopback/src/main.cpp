#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: generate a continuous CAN FD bus signal on a single board
// so you can measure it with an oscilloscope or protocol analyser.
//
// MODE_EXTERNAL_LB drives real signals on CANH/CANL through the transceiver
// while the chip ACKs its own frames — no second node required.
//
// Press any key in the Serial monitor to step through data rates:
//   2 Mbps → 4 Mbps → 5 Mbps → back to 2 Mbps

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

constexpr uint32_t FRAME_INTERVAL_MS = 10;
constexpr uint32_t STATUS_EVERY      = 100;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

struct Rate { uint32_t bps; const char* label; };
constexpr Rate RATES[] = {
    { 2000000, "125 kbps nominal / 2 Mbps data" },
    { 4000000, "125 kbps nominal / 4 Mbps data" },
    { 5000000, "125 kbps nominal / 5 Mbps data" },
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

    CanStatus s = can.configure(125000, 2000000, MODE_EXTERNAL_LB);

    Serial.println("\n==========================");
    Serial.println("  CAN FD Scope Loopback");
    Serial.println("==========================");
    Serial.printf("configure: %s  FSYS: %lu Hz\n",
                  s == CanStatus::OK ? "OK" : "FAIL", can.getFsys());
    Serial.printf("SID=0x123  DLC=8  interval=%d ms\n", FRAME_INTERVAL_MS);
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

    if (can.transmit(tx) != CanTxResult::OK)
        Serial.println("[TX failed]");
    else
        frameCount++;

    if (frameCount % STATUS_EVERY == 0)
        Serial.printf("  %s  frames=%lu\n", RATES[rateIdx].label, frameCount);

    delay(FRAME_INTERVAL_MS);
}
