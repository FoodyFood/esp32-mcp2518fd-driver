// Learning objective: impersonate a VW MEB battery on a CAN FD bus so that a
// Battery-Emulator node (or any device speaking the MEB protocol) sees a healthy,
// contactors-closed battery pack.
//
// Flash simulator to one board and monitor to the other (see platformio.ini).
// Open a Serial monitor on each at 115200 baud.
//
// The monitor role is also useful as a stand-in before you have a real
// Battery-Emulator node — it ACKs every frame and prints what it receives,
// so you can confirm the simulator is transmitting correctly.
//
// Adjust the four #defines below to simulate different battery states.

#include <Arduino.h>
#include <SPI.h>
#include "mcp2518fd_can.h"
#include "meb_frames.h"

// ---------------------------------------------------------------------------
// Simulated battery state — change these to test different conditions
// ---------------------------------------------------------------------------
#define SIM_SOC_PERCENT      65    // 0–100 %
#define SIM_VOLTAGE_V        370   // pack voltage in volts
#define SIM_MAX_CHARGE_KW    22    // max charge power in kW
#define SIM_MAX_DISCHARGE_KW 100   // max discharge power in kW

// ---------------------------------------------------------------------------
// Hardware — same pins on both boards
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

// ---------------------------------------------------------------------------
// Derived raw values — same scaling as MEB-BATTERY.cpp
// ---------------------------------------------------------------------------
static constexpr uint16_t SOC_RAW    = (SIM_SOC_PERCENT * 100) / 5;
static constexpr uint16_t VOLTAGE_RAW = (SIM_VOLTAGE_V * 4);
static constexpr uint16_t CURRENT_RAW = 16300;  // 16300 = 0 A offset
static constexpr uint16_t ENERGY_RAW  = (SIM_SOC_PERCENT * 580) / 5;
static constexpr uint16_t CAPACITY_AH = 150;

// ---------------------------------------------------------------------------
// ROLE_SIMULATOR — transmits the MEB keepalive schedule
// ---------------------------------------------------------------------------
#ifdef ROLE_SIMULATOR

static void tx(const CanMsg& msg, const char* name)
{
    if (can.transmit(msg) != CanTxResult::OK)
        Serial.printf("[SIM] TX FAIL %s\n", name);
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 2000000, MODE_NORMAL);
    if (s != CanStatus::OK) {
        Serial.println("[SIM] configure failed — halting");
        while (true) {}
    }

    // Listen for BMS_20 coming back from the remote node — confirms two-way comms
    can.setFilter(1, ID_BMS_20, 0x7FF, false);

    Serial.println("[SIM] MEB battery simulator running");
    Serial.printf("[SIM] SOC=%d%%  voltage=%dV  charge=%dkW  discharge=%dkW\n",
                  SIM_SOC_PERCENT, SIM_VOLTAGE_V,
                  SIM_MAX_CHARGE_KW, SIM_MAX_DISCHARGE_KW);
}

void loop()
{
    static uint32_t last10ms  = 0;
    static uint32_t last100ms = 0;
    static uint32_t last200ms = 0;
    static uint32_t last500ms = 0;
    static uint8_t  ctr10  = 0;
    static uint8_t  ctr100 = 0;
    static uint8_t  ctr500 = 0;

    uint32_t now = millis();

    if (now - last10ms >= 10) {
        last10ms = now;
        tx(make_BMS_20(ctr10, VOLTAGE_RAW, CURRENT_RAW), "BMS_20");
        ctr10 = (ctr10 + 1) % 16;
    }

    if (now - last100ms >= 100) {
        last100ms = now;
        tx(make_BMS_22(SOC_RAW, ENERGY_RAW),                     "BMS_22");
        tx(make_BMS_21(SIM_MAX_CHARGE_KW, SIM_MAX_DISCHARGE_KW), "BMS_21");
        tx(make_BMS_04(ctr100, CAPACITY_AH),                     "BMS_04");
        ctr100 = (ctr100 + 1) % 16;
    }

    if (now - last200ms >= 200) {
        last200ms = now;
        tx(make_NMH_Hybrid_01(), "NMH_Hybrid_01");
    }

    if (now - last500ms >= 500) {
        last500ms = now;
        tx(make_BMS_07(ctr500),  "BMS_07");
        tx(make_KN_Hybrid_01(),  "KN_Hybrid_01");
        ctr500 = (ctr500 + 1) % 16;

        if (can.hasErrors()) {
            CanError e = can.readAndClearErrors();
            Serial.printf("[SIM] BUS ERROR  tec=%d rec=%d busOff=%d\n",
                          e.tec, e.rec, e.busOff);
        }
    }

    // Receive BMS_20 from the remote node — decode BMS_mode to confirm two-way comms
    CanMsg rx = {};
    while (can.receive(rx)) {
        if (rx.id == ID_BMS_20) {
            uint8_t mode = rx.data[2] & 0x07;
            const char* label = (mode == 0) ? "standby"   :
                                (mode == 1) ? "HV_ACTIVE" :
                                (mode == 5) ? "error"     :
                                (mode == 7) ? "init"      : "other";
            Serial.printf("[SIM] RX BMS_20 mode=%d (%s)\n", mode, label);
        }
    }
}

#endif  // ROLE_SIMULATOR

// ---------------------------------------------------------------------------
// ROLE_MONITOR — ACKs all frames and prints what it receives.
// Use this to verify the simulator is transmitting before connecting a real
// Battery-Emulator node.
// ---------------------------------------------------------------------------
#ifdef ROLE_MONITOR

static const char* frameName(uint32_t id, bool ext)
{
    if (!ext) {
        switch (id) {
            case ID_BMS_20:    return "BMS_20";
            case ID_BMS_04:    return "BMS_04";
            case ID_BMS_07:    return "BMS_07";
            case ID_MSG_HYB_30: return "MSG_HYB_30";
            case ID_HVK_01:    return "HVK_01";
            default:           return "unknown";
        }
    } else {
        switch (id) {
            case ID_BMS_21:        return "BMS_21";
            case ID_BMS_22:        return "BMS_22";
            case ID_BMS_24:        return "BMS_24";
            case ID_KN_Hybrid_01:  return "KN_Hybrid_01";
            case ID_NMH_Hybrid_01: return "NMH_Hybrid_01";
            default:               return "unknown";
        }
    }
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // MODE_NORMAL so this board ACKs every frame it receives
    CanStatus s = can.configure(500000, 2000000, MODE_NORMAL);
    if (s != CanStatus::OK) {
        Serial.println("[MON] configure failed — halting");
        while (true) {}
    }

    Serial.println("[MON] MEB monitor running — waiting for frames");
}

void loop()
{
    static uint32_t lastReport = 0;
    static uint32_t frameCount = 0;

    CanMsg rx = {};
    while (can.receive(rx)) {
        frameCount++;
        uint8_t len = dlcToLen(rx.dlc);
        Serial.printf("[MON] %-16s  id=0x%08lX  dlc=%2d  %s%s  ",
                      frameName(rx.id, rx.ext),
                      (unsigned long)rx.id,
                      rx.dlc,
                      rx.fdf ? "FD " : "CAN",
                      rx.brs ? "+BRS" : "    ");
        for (int i = 0; i < min((int)len, 8); i++)
            Serial.printf("%02X ", rx.data[i]);
        if (len > 8) Serial.print("...");
        Serial.println();
    }

    // Print a heartbeat every 5 s so you know it's alive even if no frames arrive
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.printf("[MON] %lu frames received\n", frameCount);
    }
}

#endif  // ROLE_MONITOR
