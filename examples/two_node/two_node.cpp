#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Two-node CAN FD test
// Node A transmits, Node B receives via ATA6561 transceiver on the physical bus.
// Not yet implemented — placeholder for normal mode work.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    Serial.println("Two-node test — not yet implemented.");
}

void loop() {}
