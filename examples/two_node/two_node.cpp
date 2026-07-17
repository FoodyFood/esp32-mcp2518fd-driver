#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Two-node CAN FD test — MODE_NORMAL, real bus via ATA6561
//
// Both boards run this same binary.
// Send 'A' over serial to become Node A (transmitter-first).
// Send 'B' over serial to become Node B (receiver-first).
//
// Test sequence (all at 125 kbps nominal unless noted):
//   A->B  single frame    2 Mbps  8 bytes
//   A->B  3-frame burst   2 Mbps  8 bytes each
//   A->B  single frame    2 Mbps  64 bytes (DLC=15)
//   B->A  single frame    2 Mbps  8 bytes
//   B->A  3-frame burst   2 Mbps  8 bytes each
//   B->A  single frame    2 Mbps  64 bytes (DLC=15)
//   A->B  single frame    4 Mbps  8 bytes
//   A->B  single frame    5 Mbps  8 bytes
//   A->B  single frame    8 Mbps  8 bytes

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static char nodeId = 0;  // 'A' or 'B', set on first keypress

// ----------------------------------------------------------------------------

static void CHECK(const char* label, bool pass)
{
    Serial.printf("  %-44s %s\n", label, pass ? "OK" : "FAIL");
}

// Transmit one frame and print result. Returns true on success.
static bool doTx(const char* label, const CanMsg& tx)
{
    bool ok = can.transmit(tx);
    char buf[64];
    snprintf(buf, sizeof(buf), "TX %s", label);
    CHECK(buf, ok);
    return ok;
}

// Receive one frame, verify against expected, print result. Returns true on success.
static bool doRx(const char* label, const CanMsg& expected)
{
    CanMsg rx = {};
    bool ok = can.receive(rx);
    char buf[64];

    snprintf(buf, sizeof(buf), "RX %s received", label);
    CHECK(buf, ok);
    if (!ok) return false;

    snprintf(buf, sizeof(buf), "RX %s SID=0x%03X", label, expected.sid);
    CHECK(buf, rx.sid == expected.sid);

    snprintf(buf, sizeof(buf), "RX %s DLC=%d", label, expected.dlc);
    CHECK(buf, rx.dlc == expected.dlc);

    uint8_t len = dlcToLen(expected.dlc);
    bool dataOk = true;
    for (int i = 0; i < len; i++) dataOk &= (rx.data[i] == expected.data[i]);
    snprintf(buf, sizeof(buf), "RX %s all %d bytes match", label, len);
    CHECK(buf, dataOk);

    return ok && (rx.sid == expected.sid) && (rx.dlc == expected.dlc) && dataOk;
}

// Build a test frame with a known pattern.
static CanMsg makeFrame(uint16_t sid, uint8_t dlc, uint8_t seed)
{
    CanMsg msg;
    msg.sid = sid;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = dlc;
    uint8_t len = dlcToLen(dlc);
    for (int i = 0; i < len; i++) msg.data[i] = (uint8_t)(seed + i);
    return msg;
}

// Switch data bit rate, verify mode still NORMAL.
static bool switchRate(uint32_t dbtcfg, uint32_t tdc, const char* rateLabel)
{
    bool ok = can.setDataBitTiming(dbtcfg, tdc);
    char buf[64];
    snprintf(buf, sizeof(buf), "setDataBitTiming(%s) OK", rateLabel);
    CHECK(buf, ok);
    snprintf(buf, sizeof(buf), "mode=NORMAL after switch to %s", rateLabel);
    CHECK(buf, can.getMode() == MODE_NORMAL);
    return ok && (can.getMode() == MODE_NORMAL);
}

// ----------------------------------------------------------------------------

static void runNodeA()
{
    Serial.println("\n==========================");
    Serial.println("NODE A");

    // --- A->B single frame @ 2 Mbps, 8 bytes ---
    Serial.println("A->B single frame @ 2 Mbps, 8 bytes:");
    doTx("2Mbps 8B SID=0x100", makeFrame(0x100, 8, 0x10));

    // --- A->B 3-frame burst @ 2 Mbps, 8 bytes ---
    Serial.println("A->B 3-frame burst @ 2 Mbps, 8 bytes:");
    doTx("2Mbps 8B SID=0x101 frame0", makeFrame(0x101, 8, 0x20)); delay(20);
    doTx("2Mbps 8B SID=0x102 frame1", makeFrame(0x102, 8, 0x30)); delay(20);
    doTx("2Mbps 8B SID=0x103 frame2", makeFrame(0x103, 8, 0x40)); delay(20);

    // --- A->B 64-byte frame @ 2 Mbps ---
    Serial.println("A->B single frame @ 2 Mbps, DLC=15 (64 bytes):");
    doTx("2Mbps 64B SID=0x104", makeFrame(0x104, 15, 0x50));

    // --- wait for B->A frames ---
    Serial.println("A<-B single frame @ 2 Mbps, 8 bytes:");
    doRx("2Mbps 8B SID=0x200", makeFrame(0x200, 8, 0xA0));

    Serial.println("A<-B 3-frame burst @ 2 Mbps, 8 bytes:");
    doRx("2Mbps 8B SID=0x201 frame0", makeFrame(0x201, 8, 0xB0));
    doRx("2Mbps 8B SID=0x202 frame1", makeFrame(0x202, 8, 0xC0));
    doRx("2Mbps 8B SID=0x203 frame2", makeFrame(0x203, 8, 0xD0));

    Serial.println("A<-B single frame @ 2 Mbps, DLC=15 (64 bytes):");
    doRx("2Mbps 64B SID=0x204", makeFrame(0x204, 15, 0xE0));

    // --- A->B high data rates ---
    struct { uint32_t dbtcfg; uint32_t tdc; const char* label; uint8_t seed; } rates[] = {
        { DBTCFG_4M_40MHZ, TDC_4M_40MHZ, "4 Mbps", 0x41 },
        { DBTCFG_5M_40MHZ, TDC_5M_40MHZ, "5 Mbps", 0x51 },
        { DBTCFG_8M_40MHZ, TDC_8M_40MHZ, "8 Mbps", 0x81 },
    };

    for (int r = 0; r < 3; r++)
    {
        char label[48];
        snprintf(label, sizeof(label), "A->B single frame @ %s, 8 bytes:", rates[r].label);
        Serial.println(label);
        if (switchRate(rates[r].dbtcfg, rates[r].tdc, rates[r].label))
        {
            snprintf(label, sizeof(label), "%s 8B SID=0x10%d", rates[r].label, 5 + r);
            doTx(label, makeFrame(0x105 + r, 8, rates[r].seed));
        }
    }

    Serial.println();
}

static void runNodeB()
{
    Serial.println("\n==========================");
    Serial.println("NODE B");

    // --- receive A->B frames ---
    Serial.println("B<-A single frame @ 2 Mbps, 8 bytes:");
    doRx("2Mbps 8B SID=0x100", makeFrame(0x100, 8, 0x10));

    Serial.println("B<-A 3-frame burst @ 2 Mbps, 8 bytes:");
    doRx("2Mbps 8B SID=0x101 frame0", makeFrame(0x101, 8, 0x20));
    doRx("2Mbps 8B SID=0x102 frame1", makeFrame(0x102, 8, 0x30));
    doRx("2Mbps 8B SID=0x103 frame2", makeFrame(0x103, 8, 0x40));

    Serial.println("B<-A single frame @ 2 Mbps, DLC=15 (64 bytes):");
    doRx("2Mbps 64B SID=0x104", makeFrame(0x104, 15, 0x50));

    // --- B->A frames ---
    Serial.println("B->A single frame @ 2 Mbps, 8 bytes:");
    doTx("2Mbps 8B SID=0x200", makeFrame(0x200, 8, 0xA0)); delay(20);

    Serial.println("B->A 3-frame burst @ 2 Mbps, 8 bytes:");
    doTx("2Mbps 8B SID=0x201 frame0", makeFrame(0x201, 8, 0xB0)); delay(20);
    doTx("2Mbps 8B SID=0x202 frame1", makeFrame(0x202, 8, 0xC0)); delay(20);
    doTx("2Mbps 8B SID=0x203 frame2", makeFrame(0x203, 8, 0xD0)); delay(20);

    Serial.println("B->A single frame @ 2 Mbps, DLC=15 (64 bytes):");
    doTx("2Mbps 64B SID=0x204", makeFrame(0x204, 15, 0xE0));

    // --- receive A->B high rate frames ---
    struct { uint32_t dbtcfg; uint32_t tdc; const char* label; uint8_t seed; } rates[] = {
        { DBTCFG_4M_40MHZ, TDC_4M_40MHZ, "4 Mbps", 0x41 },
        { DBTCFG_5M_40MHZ, TDC_5M_40MHZ, "5 Mbps", 0x51 },
        { DBTCFG_8M_40MHZ, TDC_8M_40MHZ, "8 Mbps", 0x81 },
    };

    for (int r = 0; r < 3; r++)
    {
        char label[48];
        snprintf(label, sizeof(label), "B<-A single frame @ %s, 8 bytes:", rates[r].label);
        Serial.println(label);
        if (switchRate(rates[r].dbtcfg, rates[r].tdc, rates[r].label))
        {
            snprintf(label, sizeof(label), "%s 8B SID=0x10%d", rates[r].label, 5 + r);
            doRx(label, makeFrame(0x105 + r, 8, rates[r].seed));
        }
    }

    Serial.println();
}

// ----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(
        NBTCFG_125K_40MHZ,
        DBTCFG_2M_40MHZ,
        TDC_2M_40MHZ,
        MODE_NORMAL);

    Serial.println();
    Serial.println("==========================");
    Serial.printf("configure(MODE_NORMAL): %s\n", ok ? "OK" : "FAIL");
    Serial.printf("mode confirmed: %d (expected %d)\n", can.getMode(), MODE_NORMAL);
    Serial.println("Send 'A' (node A / COM4) or 'B' (node B / COM3) to run test...");
}

void loop()
{
    if (!Serial.available()) return;

    char c = Serial.read();
    if (c == 'A' || c == 'a') { nodeId = 'A'; runNodeA(); }
    if (c == 'B' || c == 'b') { nodeId = 'B'; runNodeB(); }
}
