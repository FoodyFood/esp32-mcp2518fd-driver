#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: send and receive text messages between two ESP32 boards over CAN FD.
//
// Flash this same binary to both boards. Open a Serial monitor on each.
// Type a message on one board and press Enter — it appears on the other.
// Both boards can transmit and receive at the same time.

constexpr uint8_t  PIN_SCK  = 33;
constexpr uint8_t  PIN_MISO = 35;
constexpr uint8_t  PIN_MOSI = 32;
constexpr uint8_t  PIN_CS   = 25;

constexpr uint32_t CHAT_ID      = 0x7E0;
constexpr uint32_t SEND_IDLE_MS = 300;  // send after this many ms of no new typing

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static char     txBuf[64];
static int      txLen       = 0;
static uint32_t txLastCharMs = 0;

// Break a string into 8-byte CAN FD frames and transmit them in order.
// A null byte in the last frame signals end-of-message to the receiver.
static void sendText(const char* text)
{
    int len = strlen(text) + 1;
    for (int offset = 0; offset < len; offset += 8)
    {
        CanMsg msg;
        msg.id  = CHAT_ID;
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

    CanStatus s = can.configure(125000, 2000000, MODE_NORMAL);

    Serial.println("\n==========================");
    Serial.println("  CAN FD Walkie-Talkie");
    Serial.println("==========================");
    Serial.printf("configure: %s\n", s == CanStatus::OK ? "OK" : "FAIL");
    Serial.println("Type a message and press Enter to send.\n");
}

void loop()
{
    // Accumulate typed characters; send on newline or after a short idle pause
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
            txLastCharMs = millis();
        }
    }
    if (txLen > 0 && millis() - txLastCharMs > SEND_IDLE_MS)
        flushTx();

    // Reassemble incoming frames back into a string and print when complete
    static char rxBuf[256];
    static int  rxLen = 0;

    CanMsg rx = {};
    while (can.available())
    {
        if (can.receive(rx) && rx.id == CHAT_ID)
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
