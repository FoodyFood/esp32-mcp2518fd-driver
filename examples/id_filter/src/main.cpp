#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// id_filter — acceptance filter demonstration
//
// Shows setFilter() and clearFilter() in internal loopback mode.
// No second node or bus required.
//
// Tests:
//   1. SID exact match  — only 0x7EC passes, 0x123 is dropped
//   2. SID range match  — 0x700-0x7FF pass, 0x123 is dropped
//   3. 29-bit EID match — only 0x1C42017B passes, 0x18DAF101 is dropped
//   4. Catch-all        — clearFilter(0) + setFilter(0,0,0,false) passes everything
//   5. Multi-filter     — filter 0 = 0x100, filter 1 = 0x200, both pass, 0x300 dropped
//
// Press any key to run. All assertions print OK or FAIL.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static void CHECK(const char* label, bool pass)
{
    Serial.printf("  %-48s %s\n", label, pass ? "OK" : "FAIL");
}

static CanMsg frame(uint32_t id, bool ext, uint8_t data0)
{
    CanMsg m;
    m.id = id; m.ext = ext; m.fdf = true; m.brs = true; m.dlc = 1;
    m.data[0] = data0;
    return m;
}

void runTest()
{
    Serial.println();
    Serial.println("==========================");

    // ------------------------------------------------------------------
    // 1. SID exact match — 0x7EC passes, 0x123 dropped
    // ------------------------------------------------------------------
    Serial.println("SID exact match (0x7EC, mask=0x7FF):");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);
    can.setFilter(0, 0x7EC, 0x7FF, false);

    can.transmit(frame(0x123, false, 0xAA));  // dropped
    can.transmit(frame(0x7EC, false, 0xBB));  // passes

    CanMsg rx = {};
    bool got = can.receive(rx, 50);
    CHECK("0x7EC received",        got && rx.id == 0x7EC && rx.data[0] == 0xBB);
    CHECK("0x123 not in FIFO",     !can.available());

    // ------------------------------------------------------------------
    // 2. SID range match — 0x700-0x7FF pass (mask=0x700), 0x123 dropped
    // ------------------------------------------------------------------
    Serial.println("SID range match (0x700-0x7FF, mask=0x700):");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);
    can.setFilter(0, 0x700, 0x700, false);

    can.transmit(frame(0x123, false, 0x11));  // dropped
    can.transmit(frame(0x7AB, false, 0x22));  // passes
    can.transmit(frame(0x7FF, false, 0x33));  // passes

    CanMsg rx2a = {}, rx2b = {};
    bool got2a = can.receive(rx2a, 50);
    bool got2b = can.receive(rx2b, 50);
    CHECK("0x7AB received",        got2a && rx2a.id == 0x7AB);
    CHECK("0x7FF received",        got2b && rx2b.id == 0x7FF);
    CHECK("0x123 not in FIFO",     !can.available());

    // ------------------------------------------------------------------
    // 3. 29-bit EID exact match — 0x1C42017B passes, 0x18DAF101 dropped
    // ------------------------------------------------------------------
    Serial.println("EID exact match (0x1C42017B, mask=0x1FFFFFFF):");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);
    can.setFilter(0, 0x1C42017BUL, 0x1FFFFFFFUL, true);

    can.transmit(frame(0x18DAF101UL, true, 0x11));  // dropped
    can.transmit(frame(0x1C42017BUL, true, 0x22));  // passes

    CanMsg rx3 = {};
    bool got3 = can.receive(rx3, 50);
    CHECK("0x1C42017B received",       got3 && rx3.ext && rx3.id == 0x1C42017BUL);
    CHECK("0x18DAF101 not in FIFO",    !can.available());

    // ------------------------------------------------------------------
    // 4. Catch-all restore — clearFilter then setFilter(0,0,0,false)
    // ------------------------------------------------------------------
    Serial.println("Catch-all restore:");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);
    can.clearFilter(0);
    can.setFilter(0, 0, 0, false);

    can.transmit(frame(0x123,       false, 0xAA));
    can.transmit(frame(0x7EC,       false, 0xBB));
    can.transmit(frame(0x1C42017BUL, true, 0xCC));

    CanMsg rx4a = {}, rx4b = {}, rx4c = {};
    CHECK("0x123 received",        can.receive(rx4a, 50) && rx4a.id == 0x123);
    CHECK("0x7EC received",        can.receive(rx4b, 50) && rx4b.id == 0x7EC);
    CHECK("EID 0x1C42017B received", can.receive(rx4c, 50) && rx4c.id == 0x1C42017BUL);

    // ------------------------------------------------------------------
    // 5. Multi-filter — filter 0 = 0x100, filter 1 = 0x200, 0x300 dropped
    // ------------------------------------------------------------------
    Serial.println("Multi-filter (filter0=0x100, filter1=0x200):");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);
    can.setFilter(0, 0x100, 0x7FF, false);
    can.setFilter(1, 0x200, 0x7FF, false);

    can.transmit(frame(0x300, false, 0x33));  // dropped
    can.transmit(frame(0x100, false, 0x11));  // passes filter 0
    can.transmit(frame(0x200, false, 0x22));  // passes filter 1

    CanMsg rx5a = {}, rx5b = {};
    bool got5a = can.receive(rx5a, 50);
    bool got5b = can.receive(rx5b, 50);
    CHECK("0x100 received",        got5a && rx5a.id == 0x100);
    CHECK("0x200 received",        got5b && rx5b.id == 0x200);
    CHECK("0x300 not in FIFO",     !can.available());

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
