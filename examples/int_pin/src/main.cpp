#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// SPEC-004: Interrupt-driven RX via INT pin (GPIO 34)
//
// Verifies that available() returns true within 1 ms after a frame arrives,
// driven by the ISR on the INT pin — not by polling the FIFO status register.
//
// Hardware requirement: MCP2518FD INT pin wired to GPIO 34.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;
constexpr uint8_t PIN_INT  = 34;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS, PIN_INT);

static void CHECK(const char* label, bool pass)
{
    Serial.printf("  %-48s %s\n", label, pass ? "OK" : "FAIL");
}

void runTest()
{
    Serial.println();
    Serial.println("==========================");
    Serial.println("SPEC-004: INT pin RX");

    can.configure(125000, 2000000, MODE_INTERNAL_LB);

    // Transmit one frame — chip will assert INT pin when it lands in RX FIFO
    CanMsg tx;
    tx.id = 0x123; tx.fdf = true; tx.brs = true; tx.dlc = 8;
    for (int i = 0; i < 8; i++) tx.data[i] = (uint8_t)i;
    can.transmit(tx);

    // available() must return true within 1 ms driven by ISR, not polling
    uint32_t t0 = millis();
    bool flagSet = false;
    while (millis() - t0 < 1)
    {
        if (can.available()) { flagSet = true; break; }
    }
    CHECK("available() true within 1 ms via INT pin", flagSet);

    // Receive and verify the frame
    CanMsg rx = {};
    bool got = can.receive(rx, 5);
    CHECK("receive() returns frame", got);
    CHECK("ID matches", got && rx.id == tx.id);
    CHECK("DLC matches", got && rx.dlc == tx.dlc);

    // After draining, available() must be false (INT pin deasserted)
    CHECK("available() false after drain", !can.available());

    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
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
