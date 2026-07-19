#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Interrupt-driven RX — INT pin (GPIO 34)
//
// Demonstrates the key benefit of interrupt-driven RX: the main loop does
// other work continuously and is never blocked waiting for CAN frames.
// Frames are captured immediately when they arrive, not on the next poll.
//
// Without INT pin (polling):
//   available() reads a register over SPI on every call — you either poll
//   constantly (wasting CPU and SPI bandwidth) or add delays (missing frames).
//
// With INT pin:
//   The MCP2518FD asserts INT low the moment a frame lands in the RX FIFO.
//   The ISR sets a flag. available() returns true immediately — no SPI read
//   needed until you actually call receive(). The loop is free to do real work.
//
// This example runs in internal loopback — no second node required.
// It transmits a frame every 100 ms and counts how many times the main loop
// runs between each TX. A high loop count confirms the loop is not blocked.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;
constexpr uint8_t PIN_INT  = 34;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS, PIN_INT);

static uint32_t gLoopCount    = 0;  // iterations of the main loop
static uint32_t gFramesRx     = 0;  // frames received via ISR-driven path
static uint32_t gLastTxMs     = 0;
static uint32_t gLastReportMs = 0;

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // Pass PIN_INT to enable interrupt-driven RX
    CanStatus s = can.configure(500000, 2000000, MODE_INTERNAL_LB);
    Serial.printf("configure: %s  FSYS: %lu Hz\n",
                  s == CanStatus::OK ? "OK" : "FAIL", can.getFsys());
    Serial.println("Running — loop count and RX rate printed every 2 s");
    Serial.println();
}

void loop()
{
    gLoopCount++;

    uint32_t now = millis();

    // Transmit one frame every 100 ms
    if (now - gLastTxMs >= 100)
    {
        gLastTxMs = now;
        CanMsg tx;
        tx.id = 0x100; tx.fdf = true; tx.brs = true; tx.dlc = 8;
        for (int i = 0; i < 8; i++) tx.data[i] = (uint8_t)(gFramesRx + i);
        can.transmit(tx);
    }

    // Receive immediately when the ISR flag fires — no polling delay
    if (can.available())
    {
        CanMsg rx;
        can.receive(rx);
        gFramesRx++;
    }

    // Print stats every 2 s
    if (now - gLastReportMs >= 2000)
    {
        gLastReportMs = now;
        Serial.printf("frames rx: %4lu   loop iterations: %6lu   loops/frame: ~%lu\n",
                      gFramesRx,
                      gLoopCount,
                      gFramesRx > 0 ? gLoopCount / gFramesRx : 0);
    }
}
