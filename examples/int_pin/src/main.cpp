#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: use the INT pin so your main loop stays free while
// CAN FD frames arrive — no polling, no missed frames, no wasted SPI reads.
//
// Without the INT pin, available() reads a register over SPI on every call.
// You either poll constantly (burning CPU) or add delays (missing frames).
//
// With the INT pin, the MCP2518FD pulls INT low the moment a frame lands in
// the RX FIFO. The driver ISR sets a flag. available() returns true immediately
// with no SPI transaction — the loop is free to do real work in between.
//
// This example runs in internal loopback so no second board is needed.
// It transmits a frame every 100 ms and counts main-loop iterations between
// each receive. A high loop count confirms the loop is never blocked waiting.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;
constexpr uint8_t PIN_INT  = 34;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS, PIN_INT);  // pass PIN_INT to enable interrupt-driven RX

static uint32_t loopCount    = 0;
static uint32_t framesRx     = 0;
static uint32_t lastTxMs     = 0;
static uint32_t lastReportMs = 0;

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 2000000, MODE_INTERNAL_LB);
    Serial.printf("configure: %s  FSYS: %lu Hz\n",
                  s == CanStatus::OK ? "OK" : "FAIL", can.getFsys());
    Serial.println("Running — loop count and RX rate printed every 2 s\n");
}

void loop()
{
    loopCount++;

    uint32_t now = millis();

    // Transmit one frame every 100 ms
    if (now - lastTxMs >= 100)
    {
        lastTxMs = now;
        CanMsg tx;
        tx.id = 0x100; tx.fdf = true; tx.brs = true; tx.dlc = 8;
        for (int i = 0; i < 8; i++) tx.data[i] = (uint8_t)(framesRx + i);
        can.transmit(tx);
    }

    // The ISR flag fires the moment a frame arrives — no polling delay
    if (can.available())
    {
        CanMsg rx;
        can.receive(rx);
        framesRx++;
    }

    // Print stats every 2 s — a high loops/frame value means the loop is free
    if (now - lastReportMs >= 2000)
    {
        lastReportMs = now;
        Serial.printf("frames rx: %4lu   loop iterations: %6lu   loops/frame: ~%lu\n",
                      framesRx,
                      loopCount,
                      framesRx > 0 ? loopCount / framesRx : 0);
    }
}
