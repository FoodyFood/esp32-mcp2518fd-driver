#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Two-node CAN FD test — MODE_NORMAL, real bus via ATA6561
//
// Both boards run this same binary. Each node operates independently —
// no coordination, no role assignment, no PC synchronisation required.
//
// Each node:
//   1. Transmits a set of frames with unique SIDs, retrying if no ACK
//      (other node may not be up yet — the chip handles retransmission,
//      we just retry at the application level if all 3 attempts fail)
//   2. Receives and verifies the other node's frames
//
// Node A owns SIDs 0x100-0x10F  (COM4)
// Node B owns SIDs 0x200-0x20F  (COM3)
//
// Send 'A' to become Node A, 'B' to become Node B.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

constexpr uint32_t TX_RETRY_DELAY_MS = 50;   // wait between retries if no ACK
constexpr uint8_t  TX_MAX_RETRIES    = 20;   // ~1s total before giving up
constexpr uint32_t RX_TIMEOUT_MS     = 2000; // generous — other node may be slow to start

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

// ----------------------------------------------------------------------------

static void CHECK(const char* label, bool pass)
{
    Serial.printf("  %-48s %s\n", label, pass ? "OK" : "FAIL");
}

static CanMsg makeFrame(uint32_t id, uint8_t dlc, uint8_t seed)
{
    CanMsg msg;
    msg.id  = id;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = dlc;
    uint8_t len = dlcToLen(dlc);
    for (int i = 0; i < len; i++) msg.data[i] = (uint8_t)(seed + i);
    return msg;
}

// Transmit with retry — if no ACK (other node not ready), wait and retry.
static bool txWithRetry(const char* label, const CanMsg& msg)
{
    char buf[64];
    for (uint8_t attempt = 0; attempt < TX_MAX_RETRIES; attempt++)
    {
        if (can.transmit(msg) == CanTxResult::OK)
        {
            snprintf(buf, sizeof(buf), "TX %s", label);
            CHECK(buf, true);
            return true;
        }
        delay(TX_RETRY_DELAY_MS);
    }
    snprintf(buf, sizeof(buf), "TX %s", label);
    CHECK(buf, false);
    return false;
}

// Receive and verify one frame.
static bool rxAndVerify(const char* label, const CanMsg& expected)
{
    char buf[64];
    CanMsg rx = {};

    // Override driver timeout with our generous two-node timeout
    bool got = can.receive(rx, RX_TIMEOUT_MS);

    snprintf(buf, sizeof(buf), "RX %s received", label);
    CHECK(buf, got);
    if (!got) return false;

    snprintf(buf, sizeof(buf), "RX %s ID=0x%03lX", label, (unsigned long)expected.id);
    CHECK(buf, rx.id == expected.id);

    snprintf(buf, sizeof(buf), "RX %s DLC=%d", label, expected.dlc);
    CHECK(buf, rx.dlc == expected.dlc);

    uint8_t len = dlcToLen(expected.dlc);
    bool dataOk = true;
    for (int i = 0; i < len; i++) dataOk &= (rx.data[i] == expected.data[i]);
    snprintf(buf, sizeof(buf), "RX %s all %d bytes match", label, len);
    CHECK(buf, dataOk);

    return got && (rx.id == expected.id) && (rx.dlc == expected.dlc) && dataOk;
}

static bool switchRate(uint32_t dataBps, const char* label)
{
    char buf[64];
    bool ok = (can.setDataRate(dataBps) == CanStatus::OK);
    snprintf(buf, sizeof(buf), "setDataRate(%s)", label);
    CHECK(buf, ok && can.getMode() == MODE_NORMAL);
    return ok && can.getMode() == MODE_NORMAL;
}

// ----------------------------------------------------------------------------

static void runNodeA()
{
    Serial.println("\n==========================");
    Serial.println("NODE A  (SIDs 0x100-0x10F)");

    // A transmits first — B may not be ready yet, txWithRetry handles that
    Serial.println("A->B @ 125kbps/2Mbps, 8 bytes:");
    txWithRetry("125k/2M 8B SID=0x100", makeFrame(0x100, 8, 0x10));

    Serial.println("A->B @ 125kbps/2Mbps, 3-frame burst:");
    txWithRetry("125k/2M 8B SID=0x101 frame0", makeFrame(0x101, 8, 0x20));
    txWithRetry("125k/2M 8B SID=0x102 frame1", makeFrame(0x102, 8, 0x30));
    txWithRetry("125k/2M 8B SID=0x103 frame2", makeFrame(0x103, 8, 0x40));

    Serial.println("A->B @ 125kbps/2Mbps, DLC=15 (64 bytes):");
    txWithRetry("125k/2M 64B SID=0x104", makeFrame(0x104, 15, 0x50));

    // Now receive B's frames
    Serial.println("A<-B @ 125kbps/2Mbps, 8 bytes:");
    rxAndVerify("125k/2M 8B SID=0x200", makeFrame(0x200, 8, 0xA0));

    Serial.println("A<-B @ 125kbps/2Mbps, 3-frame burst:");
    rxAndVerify("125k/2M 8B SID=0x201 frame0", makeFrame(0x201, 8, 0xB0));
    rxAndVerify("125k/2M 8B SID=0x202 frame1", makeFrame(0x202, 8, 0xC0));
    rxAndVerify("125k/2M 8B SID=0x203 frame2", makeFrame(0x203, 8, 0xD0));

    Serial.println("A<-B @ 125kbps/2Mbps, DLC=15 (64 bytes):");
    rxAndVerify("125k/2M 64B SID=0x204", makeFrame(0x204, 15, 0xE0));

    // High data rates — A transmits, B receives
    struct { uint32_t bps; const char* label; uint8_t seed; } rates[] = {
        { 4000000, "4 Mbps", 0x41 },
        { 5000000, "5 Mbps", 0x51 },
    };
    for (int r = 0; r < 2; r++)
    {
        char label[48];
        snprintf(label, sizeof(label), "A->B @ 125kbps/%s, 8 bytes:", rates[r].label);
        Serial.println(label);
        if (switchRate(rates[r].bps, rates[r].label))
        {
            snprintf(label, sizeof(label), "125k/%s 8B SID=0x10%d", rates[r].label, 5 + r);
            txWithRetry(label, makeFrame(0x105 + r, 8, rates[r].seed));
        }
    }

    // RX FIFO overflow test — A uses a shallow FIFO, B bursts more frames
    // than it can hold. A verifies the overflow flag is set.
    Serial.println("RX overflow (depth=4, B sends 5):");
    can.configure(125000, 2000000, MODE_NORMAL, 4);
    { CanMsg rf = {}; while (can.receive(rf, 5)) {} }
    delay(500);
    CHECK("rxOverflow after 5 frames into depth-4 FIFO",
          can.getErrors().rxOverflow);
    { CanMsg rf = {}; while (can.receive(rf, 5)) {} }

    // SPEC-005: A enters MODE_LISTEN while B transmits
    // A must receive frames from B without transmitting ACK
    Serial.println("A listen-only: receive from B without ACK:");
    can.configure(125000, 2000000, MODE_LISTEN);
    {
        // Drain any stale frames
        CanMsg rf = {};
        while (can.receive(rf, 5)) {}

        // Wait for B's listen-only test frames (SIDs 0x210-0x212)
        int rxCount = 0;
        for (int i = 0; i < 3; i++)
        {
            CanMsg r = {};
            if (can.receive(r, 2000)) rxCount++;
        }
        CHECK("A received frames in MODE_LISTEN", rxCount > 0);
        // A must not have transmitted anything — TEC stays 0
        CanError e = can.getErrors();
        CHECK("A TEC=0 in MODE_LISTEN (no TX)", e.tec == 0);
    }

    Serial.println();
}

static void runNodeB()
{
    Serial.println("\n==========================");
    Serial.println("NODE B  (SIDs 0x200-0x20F)");

    // B receives A's frames first
    Serial.println("B<-A @ 125kbps/2Mbps, 8 bytes:");
    rxAndVerify("125k/2M 8B SID=0x100", makeFrame(0x100, 8, 0x10));

    Serial.println("B<-A @ 125kbps/2Mbps, 3-frame burst:");
    rxAndVerify("125k/2M 8B SID=0x101 frame0", makeFrame(0x101, 8, 0x20));
    rxAndVerify("125k/2M 8B SID=0x102 frame1", makeFrame(0x102, 8, 0x30));
    rxAndVerify("125k/2M 8B SID=0x103 frame2", makeFrame(0x103, 8, 0x40));

    Serial.println("B<-A @ 125kbps/2Mbps, DLC=15 (64 bytes):");
    rxAndVerify("125k/2M 64B SID=0x104", makeFrame(0x104, 15, 0x50));

    // B transmits back to A
    Serial.println("B->A @ 125kbps/2Mbps, 8 bytes:");
    txWithRetry("125k/2M 8B SID=0x200", makeFrame(0x200, 8, 0xA0));

    Serial.println("B->A @ 125kbps/2Mbps, 3-frame burst:");
    txWithRetry("125k/2M 8B SID=0x201 frame0", makeFrame(0x201, 8, 0xB0));
    txWithRetry("125k/2M 8B SID=0x202 frame1", makeFrame(0x202, 8, 0xC0));
    txWithRetry("125k/2M 8B SID=0x203 frame2", makeFrame(0x203, 8, 0xD0));

    Serial.println("B->A @ 125kbps/2Mbps, DLC=15 (64 bytes):");
    txWithRetry("125k/2M 64B SID=0x204", makeFrame(0x204, 15, 0xE0));

    // High data rates — B receives A's frames
    struct { uint32_t bps; const char* label; uint8_t seed; } rates[] = {
        { 4000000, "4 Mbps", 0x41 },
        { 5000000, "5 Mbps", 0x51 },
    };
    for (int r = 0; r < 2; r++)
    {
        char label[48];
        snprintf(label, sizeof(label), "B<-A @ 125kbps/%s, 8 bytes:", rates[r].label);
        Serial.println(label);
        if (switchRate(rates[r].bps, rates[r].label))
        {
            snprintf(label, sizeof(label), "125k/%s 8B SID=0x10%d", rates[r].label, 5 + r);
            rxAndVerify(label, makeFrame(0x105 + r, 8, rates[r].seed));
        }
    }

    // B bursts 5 frames while A has a shallow FIFO and is not draining.
    Serial.println("B bursts 5 frames for A overflow test:");
    can.configure(125000, 2000000, MODE_NORMAL);
    for (int i = 0; i < 5; i++)
        txWithRetry("overflow burst", makeFrame(0x20F, 8, (uint8_t)i));

    // SPEC-005: B transmits while A is in MODE_LISTEN
    // B must see no errors (no missing ACK) because A does not transmit ACK
    // but the bus still has B's own ACK slot. With only two nodes and A passive,
    // B will see NoAck (no second node ACKing). This is the expected behaviour:
    // listen-only means A is silent — B's transmit result is not checked here,
    // but B must not go bus-off and error counters must stay reasonable.
    Serial.println("B->A listen-only: B transmits, A is MODE_LISTEN:");
    can.configure(125000, 2000000, MODE_NORMAL);
    for (int i = 0; i < 3; i++)
    {
        CanTxResult r = can.transmit(makeFrame(0x210 + i, 8, (uint8_t)(0xF0 + i)));
        char label[48];
        snprintf(label, sizeof(label), "TX 0x%03X while A listens (attempt %d)", 0x210 + i, i);
        // With A in listen mode, B is the only active node — no ACK expected
        // Result is NoAck (expected) or OK if A happened to ACK before entering listen
        CHECK(label, r == CanTxResult::NoAck || r == CanTxResult::OK);
    }
    CanError errB = can.getErrors();
    CHECK("B not bus-off after listen-only test", !errB.busOff);

    Serial.println();
}

// ----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(125000, 2000000, MODE_NORMAL) == CanStatus::OK;

    Serial.println();
    Serial.println("==========================");
    Serial.printf("configure(MODE_NORMAL): %s\n", ok ? "OK" : "FAIL");
    Serial.printf("mode confirmed: %d (expected %d)\n", can.getMode(), MODE_NORMAL);
    Serial.println("Send 'A' (COM4) or 'B' (COM3) to run test...");
}

void loop()
{
    if (!Serial.available()) return;
    char c = Serial.read();
    if (c == 'A' || c == 'a') runNodeA();
    if (c == 'B' || c == 'b') runNodeB();
}
