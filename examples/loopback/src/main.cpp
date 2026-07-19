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
    CanStatus configured = can.configure(125000, 2000000, MODE_INTERNAL_LB);

    Serial.println("configure():");
    CHECK("mode = INTERNAL_LB", can.getMode() == MODE_INTERNAL_LB);
    CHECK("configure() returned OK", configured == CanStatus::OK);
    Serial.printf("  FSYS detected: %lu Hz\n", can.getFsys());

    // ------------------------------------------------------------------
    // Transmit one frame and receive it back
    // ------------------------------------------------------------------
    CanMsg tx;
    tx.id     = 0x123;
    tx.fdf    = true;
    tx.brs    = true;
    tx.dlc    = 8;
    tx.data[0] = 0x01; tx.data[1] = 0x02; tx.data[2] = 0x03; tx.data[3] = 0x04;
    tx.data[4] = 0x05; tx.data[5] = 0x06; tx.data[6] = 0x07; tx.data[7] = 0x08;

    Serial.println("single frame @ 2 Mbps, 8 bytes:");
    CHECK("transmit() returned true", can.transmit(tx));

    CanMsg rx = {};
    CHECK("receive() returned true", can.receive(rx));
    CHECK("ID matches",   rx.id == tx.id);
    CHECK("FDF matches",  rx.fdf == tx.fdf);
    CHECK("BRS matches",  rx.brs == tx.brs);
    CHECK("DLC=8 (8 bytes)", rx.dlc == tx.dlc);
    bool dataOk = true;
    for (int i = 0; i < 8; i++) dataOk &= (rx.data[i] == tx.data[i]);
    CHECK("all 8 bytes match", dataOk);

    // ------------------------------------------------------------------
    // Multi-frame: 3 frames with different SIDs and payloads
    // ------------------------------------------------------------------
    Serial.println("multi-frame @ 2 Mbps, 3x 8 bytes:");

    struct { uint32_t id; uint8_t d0; uint8_t d7; } cases[3] = {
        { 0x001, 0xAA, 0x11 },
        { 0x7FF, 0x12, 0xF0 },
        { 0x456, 0xDE, 0xBE },
    };

    for (int i = 0; i < 3; i++)
    {
        CanMsg t;
        t.id = cases[i].id;
        t.fdf = true; t.brs = true; t.dlc = 8;
        for (int j = 0; j < 8; j++) t.data[j] = cases[i].d0 + j;

        bool txOk = can.transmit(t);
        CanMsg r = {};
        bool rxOk = can.receive(r);

        bool match = rxOk
                  && r.id == t.id
                  && r.fdf && r.brs
                  && r.dlc == 8;
        for (int j = 0; j < 8 && match; j++) match &= (r.data[j] == t.data[j]);

        char label[48];
        snprintf(label, sizeof(label), "frame %d ID=0x%03lX 8 bytes", i, (unsigned long)cases[i].id);
        CHECK(label, txOk && match);
    }

    // ------------------------------------------------------------------
    // Bitrate switch: change to 2 Mbps (same rate, different TDC config
    // to exercise the setDataBitTiming path), send one frame
    // ------------------------------------------------------------------
    Serial.println("setDataBitTiming() @ 2 Mbps, 8 bytes:");

    bool switched = (can.setDataRate(2000000) == CanStatus::OK);
    CHECK("setDataRate(2 Mbps) returned OK", switched);
    CHECK("mode = INTERNAL_LB after switch", can.getMode() == MODE_INTERNAL_LB);

    CanMsg tx2;
    tx2.id = 0x321; tx2.fdf = true; tx2.brs = true; tx2.dlc = 8;
    for (int i = 0; i < 8; i++) tx2.data[i] = 0x10 + i;

    CHECK("transmit() 8 bytes @ 2 Mbps", can.transmit(tx2));
    CanMsg rx2 = {};
    CHECK("receive() 8 bytes @ 2 Mbps", can.receive(rx2));
    CHECK("ID matches", rx2.id == tx2.id);
    bool data2Ok = true;
    for (int i = 0; i < 8; i++) data2Ok &= (rx2.data[i] == tx2.data[i]);
    CHECK("all 8 bytes match", data2Ok);

    // ------------------------------------------------------------------
    // 64-byte CAN FD frame — DLC=15, all 64 bytes verified
    // ------------------------------------------------------------------
    Serial.println("single frame @ 2 Mbps, DLC=15 (64 bytes):");

    CanMsg tx3;
    tx3.id = 0x555; tx3.fdf = true; tx3.brs = true; tx3.dlc = 15;
    for (int i = 0; i < 64; i++) tx3.data[i] = (uint8_t)(i ^ 0xA5);

    CHECK("transmit() 64 bytes @ 2 Mbps", can.transmit(tx3));
    CanMsg rx3 = {};
    CHECK("receive() 64 bytes @ 2 Mbps", can.receive(rx3));
    CHECK("ID matches",   rx3.id == tx3.id);
    CHECK("DLC=15 (64 bytes)", rx3.dlc == 15);
    bool data3Ok = true;
    for (int i = 0; i < 64; i++) data3Ok &= (rx3.data[i] == tx3.data[i]);
    CHECK("all 64 bytes match", data3Ok);

    // ------------------------------------------------------------------
    // Higher data rates: 4 Mbps, 5 Mbps, 8 Mbps
    // Each switches rate, sends one 8-byte frame, verifies all bytes.
    // ------------------------------------------------------------------
    struct { const char* label; uint32_t bps; } rates[] = {
        { "4 Mbps", 4000000 },
        { "5 Mbps", 5000000 },
    };

    for (int r = 0; r < 2; r++)
    {
        char label[56];
        Serial.printf("setDataRate() @ %s, 8 bytes:\n", rates[r].label);

        bool sw = (can.setDataRate(rates[r].bps) == CanStatus::OK);
        snprintf(label, sizeof(label), "setDataRate(%s) returned OK", rates[r].label);
        CHECK(label, sw);

        CanMsg tx4;
        tx4.id = 0x100 + r; tx4.fdf = true; tx4.brs = true; tx4.dlc = 8;
        for (int i = 0; i < 8; i++) tx4.data[i] = (uint8_t)(0x30 + r * 8 + i);

        bool txOk = can.transmit(tx4);
        CanMsg rx4 = {};
        bool rxOk = can.receive(rx4);
        bool dataOk = rxOk && rx4.id == tx4.id && rx4.dlc == 8;
        for (int i = 0; i < 8 && dataOk; i++) dataOk &= (rx4.data[i] == tx4.data[i]);

        snprintf(label, sizeof(label), "transmit() + receive() 8 bytes @ %s", rates[r].label);
        CHECK(label, txOk && rxOk);
        snprintf(label, sizeof(label), "all 8 bytes match @ %s", rates[r].label);
        CHECK(label, dataOk);
    }

    // ------------------------------------------------------------------
    // 8 Mbps not achievable at 20 MHz — verify RATE_NOT_ACHIEVABLE
    // ------------------------------------------------------------------
    Serial.println("setDataRate(8 Mbps) on 20 MHz — expect RATE_NOT_ACHIEVABLE:");
    CHECK("setDataRate(8M) = RATE_NOT_ACHIEVABLE",
          can.setDataRate(8000000) == CanStatus::RATE_NOT_ACHIEVABLE);
    CHECK("mode still INTERNAL_LB after failed setDataRate",
          can.getMode() == MODE_INTERNAL_LB);

    // ------------------------------------------------------------------
    // Raw API — 40 MHz presets forced onto 20 MHz hardware.
    // Actual wire rates are half the label (e.g. "8 Mbps" preset runs at
    // 4 Mbps on 20 MHz), but loopback passes because TX and RX share the
    // same register values. Exercises configureRaw() + setDataBitTimingRaw().
    // ------------------------------------------------------------------
    Serial.println("configureRaw() with 40 MHz presets (raw API path):");
    CanStatus rawOk = can.configureRaw(
        NBTCFG_125K_40MHZ, DBTCFG_2M_40MHZ, TDC_2M_40MHZ, MODE_INTERNAL_LB);
    CHECK("configureRaw() returned OK", rawOk == CanStatus::OK);
    CHECK("mode = INTERNAL_LB", can.getMode() == MODE_INTERNAL_LB);

    struct { const char* label; uint32_t dbtcfg; uint32_t tdc; } rawRates[] = {
        { "40MHz/1M preset",  DBTCFG_1M_40MHZ,  TDC_1M_40MHZ  },
        { "40MHz/2M preset",  DBTCFG_2M_40MHZ,  TDC_2M_40MHZ  },
        { "40MHz/4M preset",  DBTCFG_4M_40MHZ,  TDC_4M_40MHZ  },
        { "40MHz/5M preset",  DBTCFG_5M_40MHZ,  TDC_5M_40MHZ  },
        { "40MHz/8M preset",  DBTCFG_8M_40MHZ,  TDC_8M_40MHZ  },
    };

    for (int r = 0; r < 5; r++)
    {
        char label[56];
        bool sw = (can.setDataBitTimingRaw(rawRates[r].dbtcfg, rawRates[r].tdc) == CanStatus::OK);
        snprintf(label, sizeof(label), "setDataBitTimingRaw(%s) OK", rawRates[r].label);
        CHECK(label, sw);

        CanMsg t;
        t.id = 0x200 + r; t.fdf = true; t.brs = true; t.dlc = 8;
        for (int i = 0; i < 8; i++) t.data[i] = (uint8_t)(0x50 + r * 8 + i);

        bool txOk = can.transmit(t);
        CanMsg rx5 = {};
        bool rxOk = can.receive(rx5);
        bool dataOk = rxOk && rx5.id == t.id && rx5.dlc == 8;
        for (int i = 0; i < 8 && dataOk; i++) dataOk &= (rx5.data[i] == t.data[i]);

        snprintf(label, sizeof(label), "loopback 8 bytes (%s)", rawRates[r].label);
        CHECK(label, txOk && dataOk);
    }

    // ------------------------------------------------------------------
    // 29-bit Extended ID (EID) loopback
    // ------------------------------------------------------------------
    Serial.println("29-bit EID loopback @ 2 Mbps:");
    can.configure(125000, 2000000, MODE_INTERNAL_LB);

    CanMsg txE;
    txE.id  = 0x1C420017UL;
    txE.ext = true;
    txE.fdf = true;
    txE.brs = true;
    txE.dlc = 8;
    for (int i = 0; i < 8; i++) txE.data[i] = (uint8_t)(0xE0 + i);

    CHECK("EID transmit() returned true", can.transmit(txE));
    CanMsg rxE = {};
    CHECK("EID receive() returned true", can.receive(rxE));
    CHECK("EID ext=true",               rxE.ext == true);
    CHECK("EID id=0x1C420017",          rxE.id  == 0x1C420017UL);
    CHECK("EID FDF matches",            rxE.fdf == txE.fdf);
    CHECK("EID BRS matches",            rxE.brs == txE.brs);
    CHECK("EID DLC=8",                  rxE.dlc == 8);
    bool eidDataOk = true;
    for (int i = 0; i < 8; i++) eidDataOk &= (rxE.data[i] == txE.data[i]);
    CHECK("EID all 8 bytes match",      eidDataOk);

    // SID frame immediately after — confirm ext=false and id preserved
    CanMsg txS;
    txS.id  = 0x123;
    txS.ext = false;
    txS.fdf = true;
    txS.brs = true;
    txS.dlc = 8;
    for (int i = 0; i < 8; i++) txS.data[i] = (uint8_t)(0xA0 + i);

    CHECK("SID after EID transmit() true", can.transmit(txS));
    CanMsg rxS = {};
    CHECK("SID after EID receive() true",  can.receive(rxS));
    CHECK("SID ext=false",                 rxS.ext == false);
    CHECK("SID id=0x123",                  rxS.id  == 0x123UL);
    bool sidDataOk = true;
    for (int i = 0; i < 8; i++) sidDataOk &= (rxS.data[i] == txS.data[i]);
    CHECK("SID all 8 bytes match",         sidDataOk);

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
