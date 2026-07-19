#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Bus monitor — two nodes continuously transmitting and receiving
//
// Both boards run this binary. Each node starts transmitting immediately
// on boot — no serial input required. Good for scope measurements and
// bus load testing with genuine two-node ACKs.
//
// Node A (node_a env, COM4): TX SID=0x100
// Node B (node_b env, COM3): TX SID=0x200
//
// Frame payload: bytes 0-3 = uint32_t counter, bytes 4-7 = 0xDEADBEEF
// Rate: 125 kbps nominal / 2 Mbps data
// Press '+'/'-' to adjust TX interval, 's' for status

#ifndef NODE_SID
#error "NODE_SID must be defined — use env:node_a or env:node_b"
#endif

constexpr uint8_t  PIN_SCK  = 33;
constexpr uint8_t  PIN_MISO = 35;
constexpr uint8_t  PIN_MOSI = 32;
constexpr uint8_t  PIN_CS   = 25;

constexpr uint32_t TX_INTERVAL_MIN_MS = 5;
constexpr uint32_t TX_INTERVAL_MAX_MS = 1000;
constexpr uint32_t TX_INTERVAL_STEP   = 5;
constexpr uint32_t STATUS_INTERVAL_MS = 5000;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static uint32_t txInterval = 50;
static uint32_t txCounter  = 0;
static uint32_t rxCounter  = 0;
static uint32_t rxErrors   = 0;
static uint32_t lastTx     = 0;
static uint32_t lastStatus = 0;

static CanMsg makeTxFrame()
{
    CanMsg msg;
    msg.id    = NODE_SID;
    msg.fdf   = true;
    msg.brs   = true;
    msg.dlc   = 8;
    msg.data[0] = (txCounter)       & 0xFF;
    msg.data[1] = (txCounter >> 8)  & 0xFF;
    msg.data[2] = (txCounter >> 16) & 0xFF;
    msg.data[3] = (txCounter >> 24) & 0xFF;
    msg.data[4] = 0xEF;
    msg.data[5] = 0xBE;
    msg.data[6] = 0xAD;
    msg.data[7] = 0xDE;
    return msg;
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(125000, 2000000, MODE_NORMAL) == CanStatus::OK;

    Serial.println();
    Serial.println("==========================");
    Serial.println("  CAN FD Bus Monitor");
    Serial.println("==========================");
    Serial.printf("SID=0x%03X  configure: %s\n", (uint16_t)NODE_SID, ok ? "OK" : "FAIL");
    Serial.println("'+'/'-' TX interval  's' status");
}

void loop()
{
    while (Serial.available())
    {
        char c = Serial.read();
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
        if (c == 's')
            Serial.printf("  SID=0x%03X  TX=%lu  RX=%lu  RX_ERR=%lu  interval=%lums\n",
                          (uint16_t)NODE_SID, txCounter, rxCounter, rxErrors, txInterval);
    }

    uint32_t now = millis();

    if (now - lastTx >= txInterval)
    {
        lastTx = now;
        CanMsg tx = makeTxFrame();
        if (can.transmit(tx) == CanTxResult::OK)
            txCounter++;
        else
            Serial.printf("  [TX FAIL count=%lu]\n", txCounter);
    }

    CanMsg rx = {};
    if (can.receive(rx))
    {
        bool ok = rx.data[4] == 0xEF && rx.data[5] == 0xBE &&
                  rx.data[6] == 0xAD && rx.data[7] == 0xDE;
        if (ok)
        {
            rxCounter++;
            uint32_t cnt = rx.data[0] | ((uint32_t)rx.data[1] << 8)
                         | ((uint32_t)rx.data[2] << 16) | ((uint32_t)rx.data[3] << 24);
            Serial.printf("  RX ID=0x%03lX count=%lu\n", (unsigned long)rx.id, cnt);
        }
        else
        {
            rxErrors++;
            Serial.printf("  RX ID=0x%03lX INTEGRITY FAIL\n", (unsigned long)rx.id);
        }
    }

    if (now - lastStatus >= STATUS_INTERVAL_MS)
    {
        lastStatus = now;
        Serial.printf("  SID=0x%03X  TX=%lu  RX=%lu  RX_ERR=%lu  interval=%lums\n",
                      (uint16_t)NODE_SID, txCounter, rxCounter, rxErrors, txInterval);
    }
}
