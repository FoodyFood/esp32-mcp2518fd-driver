#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Bus monitor — two nodes continuously transmitting and receiving
//
// Both boards run this same binary in MODE_NORMAL.
// Each node transmits a counter frame every TX_INTERVAL_MS and prints
// any frames it receives. Good for scope measurements of real two-node
// bus traffic with genuine ACKs from the other node.
//
// Node A owns SID=0x100, Node B owns SID=0x200.
// Send 'A' or 'B' over serial to assign role.
//
// Frame payload: 8 bytes — first 4 bytes are a uint32_t frame counter,
// last 4 bytes are 0xDEADBEEF as a integrity marker.
//
// Rate: 125 kbps nominal / 2 Mbps data.
// Press '+'/'-' to increase/decrease TX interval.

constexpr uint8_t  PIN_SCK  = 33;
constexpr uint8_t  PIN_MISO = 35;
constexpr uint8_t  PIN_MOSI = 32;
constexpr uint8_t  PIN_CS   = 25;

constexpr uint32_t TX_INTERVAL_MIN_MS = 5;
constexpr uint32_t TX_INTERVAL_MAX_MS = 1000;
constexpr uint32_t TX_INTERVAL_STEP   = 5;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static char     nodeId        = 0;
static uint16_t txSid         = 0;
static uint32_t txInterval    = 50;
static uint32_t txCounter     = 0;
static uint32_t rxCounter     = 0;
static uint32_t rxErrors      = 0;
static uint32_t lastTx        = 0;
static uint32_t lastStatus    = 0;

static CanMsg makeTxFrame()
{
    CanMsg msg;
    msg.sid = txSid;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;
    // bytes 0-3: counter (little-endian)
    msg.data[0] = (txCounter)       & 0xFF;
    msg.data[1] = (txCounter >> 8)  & 0xFF;
    msg.data[2] = (txCounter >> 16) & 0xFF;
    msg.data[3] = (txCounter >> 24) & 0xFF;
    // bytes 4-7: integrity marker 0xDEADBEEF
    msg.data[4] = 0xEF;
    msg.data[5] = 0xBE;
    msg.data[6] = 0xAD;
    msg.data[7] = 0xDE;
    return msg;
}

static bool checkIntegrity(const CanMsg& rx)
{
    return rx.data[4] == 0xEF &&
           rx.data[5] == 0xBE &&
           rx.data[6] == 0xAD &&
           rx.data[7] == 0xDE;
}

static void printStatus()
{
    Serial.printf("  Node=%c  SID=0x%03X  TX=%lu  RX=%lu  RX_ERR=%lu  interval=%lums\n",
                  nodeId, txSid, txCounter, rxCounter, rxErrors, txInterval);
}

static void startNode(char id)
{
    nodeId = id;
    txSid  = (id == 'A') ? 0x100 : 0x200;
    txCounter = rxCounter = rxErrors = 0;
    Serial.printf("\nNode %c started — TX SID=0x%03X  RX SID=0x%03X\n",
                  nodeId, txSid, (id == 'A') ? 0x200 : 0x100);
    Serial.println("  '+'/'-' to adjust TX interval, 's' for status");
}

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
    Serial.println("  CAN FD Bus Monitor");
    Serial.println("==========================");
    Serial.printf("configure(MODE_NORMAL): %s\n", ok ? "OK" : "FAIL");
    Serial.println("Send 'A' (COM4) or 'B' (COM3) to start.");
}

void loop()
{
    // Handle serial commands
    while (Serial.available())
    {
        char c = Serial.read();
        if ((c == 'A' || c == 'a') && nodeId == 0) startNode('A');
        if ((c == 'B' || c == 'b') && nodeId == 0) startNode('B');
        if (c == '+' && txInterval < TX_INTERVAL_MAX_MS)
        {
            txInterval += TX_INTERVAL_STEP;
            Serial.printf("  TX interval -> %lums\n", txInterval);
        }
        if (c == '-' && txInterval > TX_INTERVAL_MIN_MS)
        {
            txInterval -= TX_INTERVAL_STEP;
            Serial.printf("  TX interval -> %lums\n", txInterval);
        }
        if (c == 's') printStatus();
    }

    if (nodeId == 0) return;

    uint32_t now = millis();

    // Transmit on interval
    if (now - lastTx >= txInterval)
    {
        lastTx = now;
        CanMsg tx = makeTxFrame();
        if (can.transmit(tx))
            txCounter++;
        else
            Serial.printf("  [TX FAIL counter=%lu]\n", txCounter);
    }

    // Receive any incoming frames
    CanMsg rx = {};
    if (can.receive(rx))
    {
        if (checkIntegrity(rx))
        {
            rxCounter++;
            uint32_t theirCount = rx.data[0] | ((uint32_t)rx.data[1] << 8)
                                | ((uint32_t)rx.data[2] << 16) | ((uint32_t)rx.data[3] << 24);
            Serial.printf("  RX SID=0x%03X count=%lu\n", rx.sid, theirCount);
        }
        else
        {
            rxErrors++;
            Serial.printf("  RX SID=0x%03X INTEGRITY FAIL\n", rx.sid);
        }
    }

    // Print status every 5 seconds
    if (now - lastStatus >= 5000)
    {
        lastStatus = now;
        printStatus();
    }
}
