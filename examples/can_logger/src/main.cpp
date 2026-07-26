#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: passively monitor a CAN bus without disturbing it.
//
// MODE_LISTEN puts the chip in receive-only mode — it never transmits ACK bits
// or error frames, so it is completely invisible to the other nodes on the bus.
// Every frame is captured with a hardware timestamp accurate to 50 ns.
//
// Connect this board to any active CAN FD bus (or run alongside bus_monitor).
// Every frame that arrives is printed to Serial in a candump-style format:
//
//   t=12507.6 ms  ID=0x100  FD BRS  DLC=8  01 02 03 04 05 06 07 08
//
// No second board is required — the logger works on any live bus.
// To test with a single board, change MODE_LISTEN to MODE_INTERNAL_LB.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

// Convert a raw TBC timestamp to milliseconds.
// TBC increments every 50 ns at 20 MHz (TBCPRE=0).
static float tbcToMs(uint32_t tbc) { return tbc * 0.00005f; }

static void printFrame(const CanMsg& msg)
{
    // Timestamp
    Serial.printf("t=%10.3f ms  ", tbcToMs(msg.timestamp));

    // ID — mark extended frames with (E)
    if (msg.ext)
        Serial.printf("ID=0x%08lX(E)  ", (unsigned long)msg.id);
    else
        Serial.printf("ID=0x%03lX       ", (unsigned long)msg.id);

    // Frame type flags
    if (msg.fdf) Serial.print("FD ");
    if (msg.brs) Serial.print("BRS ");
    if (!msg.fdf) Serial.print("CAN ");

    // DLC and payload
    uint8_t len = dlcToLen(msg.dlc);
    Serial.printf(" DLC=%d  ", msg.dlc);
    for (int i = 0; i < len; i++)
        Serial.printf("%02X ", msg.data[i]);

    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // enableTimestamp=true captures a hardware timestamp on every received frame.
    // MODE_LISTEN means this board never transmits — completely passive.
    CanConfig cfg;
    cfg.enableTimestamp = true;
    CanStatus s = can.configure(500000, 2000000, MODE_LISTEN, cfg);

    Serial.println("\n==========================");
    Serial.println("  CAN FD Logger");
    Serial.println("==========================");
    Serial.printf("configure: %s  FSYS: %lu Hz\n",
                  s == CanStatus::OK ? "OK" : "FAIL", can.getFsys());
    Serial.println("Listening — all frames printed below\n");
}

void loop()
{
    CanMsg rx = {};
    if (can.receive(rx))
        printFrame(rx);
}
