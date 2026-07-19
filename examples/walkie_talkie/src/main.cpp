#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Walkie-talkie — text chat between two nodes over CAN FD
//
// Both boards run this same binary.
// Type text in the Serial monitor and press Enter — it is sent as one or
// more CAN FD frames to the other node, which prints it to its Serial monitor.
//
// Encoding: each frame carries up to 8 bytes of ASCII text (DLC=8).
//           A null terminator in the payload signals end of message.
//           SID=0x7E0 for all chat frames.
//
// Rate: 125 kbps nominal / 2 Mbps data.
// No role assignment — both nodes transmit and receive simultaneously.

constexpr uint8_t  PIN_SCK  = 33;
constexpr uint8_t  PIN_MISO = 35;
constexpr uint8_t  PIN_MOSI = 32;
constexpr uint8_t  PIN_CS   = 25;

constexpr uint32_t CHAT_SID = 0x7E0;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static char     txBuf[64];
static int      txLen    = 0;
static uint32_t txLastChar = 0;  // millis() of last received serial char
constexpr uint32_t TX_IDLE_MS = 300;  // send after 300ms of no new chars

// Send a null-terminated string as a sequence of 8-byte CAN FD frames.
static void sendText(const char* text)
{
    int len = strlen(text) + 1;  // include null terminator
    for (int offset = 0; offset < len; offset += 8)
    {
        CanMsg msg;
        msg.id  = CHAT_SID;
        msg.fdf = true;
        msg.brs = true;
        msg.dlc = 8;
        memset(msg.data, 0, 8);
        memcpy(msg.data, text + offset, min(8, len - offset));
        if (can.transmit(msg) != CanTxResult::OK)
            Serial.println("[TX failed]");
    }
}

static void flushTx()
{
    if (txLen > 0)
    {
        txBuf[txLen] = '\0';
        Serial.printf("ME: %s\n", txBuf);
        sendText(txBuf);
        txLen = 0;
    }
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    bool ok = can.configure(125000, 2000000, MODE_NORMAL) == CanStatus::OK;

    Serial.println();
    Serial.println("==========================");
    Serial.println("  CAN FD Walkie-Talkie");
    Serial.println("==========================");
    Serial.printf("configure(MODE_NORMAL): %s\n", ok ? "OK" : "FAIL");
    Serial.println("Type a message and press Enter to send.");
    Serial.println();
}

void loop()
{
    // Accumulate serial input — send on newline or after 300ms idle
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n' || c == '\r')
        {
            flushTx();
        }
        else if (txLen < (int)sizeof(txBuf) - 1)
        {
            txBuf[txLen++] = c;
            txLastChar = millis();
        }
    }
    if (txLen > 0 && millis() - txLastChar > TX_IDLE_MS)
        flushTx();

    // Non-blocking receive — reassemble incoming frames into messages
    static char rxBuf[256];
    static int  rxLen = 0;

    CanMsg rx = {};
    while (can.available())
    {
        if (can.receive(rx) && rx.id == CHAT_SID)
        {
            for (int i = 0; i < 8; i++)
            {
                char c = (char)rx.data[i];
                if (c == '\0')
                {
                    rxBuf[rxLen] = '\0';
                    if (rxLen > 0)
                        Serial.printf("THEM: %s\n", rxBuf);
                    rxLen = 0;
                    break;
                }
                if (rxLen < (int)sizeof(rxBuf) - 1)
                    rxBuf[rxLen++] = c;
            }
        }
    }
}
