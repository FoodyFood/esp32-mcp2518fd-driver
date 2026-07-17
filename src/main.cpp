#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass       spi(VSPI);
MCP2518Driver  can(spi, PIN_CS);

static void CHECK(const char* label, bool pass)
{
    Serial.printf("  %-40s %s\n", label, pass ? "OK" : "FAIL");
}

void runTest()
{
    Serial.println();
    Serial.println("==========================");

    // ------------------------------------------------------------------
    // Configure: 125 kbps nominal, 2 Mbps data with TDC, internal loopback
    // ------------------------------------------------------------------
    bool configured = can.configure(
        NBTCFG_125K_40MHZ,
        DBTCFG_2M_40MHZ,
        TDC_2M_40MHZ,
        MODE_INTERNAL_LB);

    Serial.println("configure():");
    CHECK("mode = INTERNAL_LB", can.getMode() == MODE_INTERNAL_LB);
    CHECK("configure() returned true", configured);

    // ------------------------------------------------------------------
    // Transmit one frame and receive it back
    // ------------------------------------------------------------------
    CanMsg tx;
    tx.sid    = 0x123;
    tx.fdf    = true;
    tx.brs    = true;
    tx.dlc    = 8;
    tx.data[0] = 0x01; tx.data[1] = 0x02; tx.data[2] = 0x03; tx.data[3] = 0x04;
    tx.data[4] = 0x05; tx.data[5] = 0x06; tx.data[6] = 0x07; tx.data[7] = 0x08;

    Serial.println("transmit() + receive() single frame:");
    CHECK("transmit() returned true", can.transmit(tx));

    CanMsg rx = {};
    CHECK("receive() returned true", can.receive(rx));
    CHECK("SID matches",  rx.sid == tx.sid);
    CHECK("FDF matches",  rx.fdf == tx.fdf);
    CHECK("BRS matches",  rx.brs == tx.brs);
    CHECK("DLC matches",  rx.dlc == tx.dlc);
    bool dataOk = true;
    for (int i = 0; i < 8; i++) dataOk &= (rx.data[i] == tx.data[i]);
    CHECK("data matches", dataOk);

    // ------------------------------------------------------------------
    // Multi-frame: 3 frames with different SIDs and payloads
    // ------------------------------------------------------------------
    Serial.println("multi-frame (3 frames, 2 Mbps):");

    struct { uint16_t sid; uint8_t d0; uint8_t d7; } cases[3] = {
        { 0x001, 0xAA, 0x11 },
        { 0x7FF, 0x12, 0xF0 },
        { 0x456, 0xDE, 0xBE },
    };

    for (int i = 0; i < 3; i++)
    {
        CanMsg t;
        t.sid = cases[i].sid;
        t.fdf = true; t.brs = true; t.dlc = 8;
        for (int j = 0; j < 8; j++) t.data[j] = cases[i].d0 + j;

        bool txOk = can.transmit(t);
        CanMsg r = {};
        bool rxOk = can.receive(r);

        bool match = rxOk
                  && r.sid == t.sid
                  && r.fdf && r.brs
                  && r.dlc == 8
                  && r.data[0] == t.data[0]
                  && r.data[7] == t.data[7];

        char label[48];
        snprintf(label, sizeof(label), "frame %d SID=0x%03X", i, cases[i].sid);
        CHECK(label, txOk && match);
    }

    // ------------------------------------------------------------------
    // Bitrate switch: change to 2 Mbps (same rate, different TDC config
    // to exercise the setDataBitTiming path), send one frame
    // ------------------------------------------------------------------
    Serial.println("setDataBitTiming() + transmit/receive:");

    bool switched = can.setDataBitTiming(DBTCFG_2M_40MHZ, TDC_2M_40MHZ);
    CHECK("setDataBitTiming() returned true", switched);
    CHECK("mode = INTERNAL_LB after switch", can.getMode() == MODE_INTERNAL_LB);

    CanMsg tx2;
    tx2.sid = 0x321; tx2.fdf = true; tx2.brs = true; tx2.dlc = 8;
    for (int i = 0; i < 8; i++) tx2.data[i] = 0x10 + i;

    CHECK("transmit() after bitrate switch", can.transmit(tx2));
    CanMsg rx2 = {};
    CHECK("receive() after bitrate switch", can.receive(rx2));
    CHECK("SID matches after switch", rx2.sid == tx2.sid);
    bool data2Ok = true;
    for (int i = 0; i < 8; i++) data2Ok &= (rx2.data[i] == tx2.data[i]);
    CHECK("data matches after switch", data2Ok);

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
