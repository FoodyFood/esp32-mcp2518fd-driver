#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: use the MCP2518FD as a plain CAN 2.0B controller to talk
// to legacy devices that don't support CAN FD.
//
// MODE_CLASSIC puts the chip into Normal CAN 2.0 mode — no FD frames, no bit
// rate switching, just standard 8-byte frames at a fixed bit rate. Any classic
// CAN device on the bus will communicate with it normally.
//
// This example simulates a simple vehicle — one board acts as the powertrain ECU
// broadcasting RPM, speed and coolant temperature, the other acts as a dashboard
// reading and displaying those values.
//
// Flash ecu to one board and dashboard to the other (see platformio.ini).
// Open a Serial monitor on each at 115200 baud.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

// Classic CAN IDs — typical automotive convention
constexpr uint32_t ID_ENGINE   = 0x0C0;  // RPM + throttle, 10 ms
constexpr uint32_t ID_SPEED    = 0x0D0;  // wheel speed, 10 ms
constexpr uint32_t ID_COOLANT  = 0x130;  // coolant temp + fan, 100 ms

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

static CanMsg classicFrame(uint32_t id)
{
    CanMsg msg;
    msg.id  = id;
    msg.fdf = false;  // classic CAN — no FD
    msg.brs = false;
    msg.dlc = 8;
    return msg;
}

// ---------------------------------------------------------------------------
// ECU — broadcasts engine, speed and temperature frames
// ---------------------------------------------------------------------------

#ifdef ROLE_ECU

// Simulated vehicle state — values change slowly over time
static uint16_t rpm      = 800;
static uint8_t  throttle = 0;
static uint16_t speed    = 0;
static int8_t   coolant  = 20;
static bool     fanOn    = false;

static void updateVehicleState()
{
    // Simulate gentle acceleration up to 3000 rpm then back to idle
    static bool accelerating = true;
    if (accelerating) { rpm += 10; throttle = (rpm - 800) / 22; }
    else              { rpm -= 10; throttle = 0; }
    if (rpm >= 3000) accelerating = false;
    if (rpm <= 800)  { rpm = 800; accelerating = true; }

    speed   = (rpm > 1000) ? (rpm - 1000) / 20 : 0;
    coolant = (int8_t)min(95, coolant + (rpm > 2000 ? 1 : 0));
    fanOn   = (coolant >= 90);
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // dataBps=0 — ignored in MODE_CLASSIC, no data phase
    CanStatus s = can.configure(500000, 0, MODE_CLASSIC);

    Serial.println("\n==========================");
    Serial.println("  Classic CAN ECU");
    Serial.println("==========================");
    Serial.printf("configure: %s\n", s == CanStatus::OK ? "OK" : "FAIL");
    Serial.println("Broadcasting engine, speed and temperature...\n");
}

void loop()
{
    static uint32_t lastFast = 0;
    static uint32_t lastSlow = 0;

    uint32_t now = millis();
    updateVehicleState();

    // 10 ms frames — engine and speed
    if (now - lastFast >= 10)
    {
        lastFast = now;

        CanMsg engine = classicFrame(ID_ENGINE);
        engine.data[0] = (rpm >> 8) & 0xFF;
        engine.data[1] =  rpm       & 0xFF;
        engine.data[2] = throttle;
        can.transmit(engine);

        CanMsg spd = classicFrame(ID_SPEED);
        spd.data[0] = (speed >> 8) & 0xFF;
        spd.data[1] =  speed       & 0xFF;
        can.transmit(spd);
    }

    // 100 ms frame — coolant temperature
    if (now - lastSlow >= 100)
    {
        lastSlow = now;

        CanMsg temp = classicFrame(ID_COOLANT);
        temp.data[0] = (uint8_t)(coolant + 40);  // offset encoding: 0 = -40 C
        temp.data[1] = fanOn ? 1 : 0;
        can.transmit(temp);

        Serial.printf("ECU  rpm=%4d  speed=%3d km/h  coolant=%3d C  fan=%s\n",
                      rpm, speed, coolant, fanOn ? "ON" : "off");
    }
}

#endif  // ROLE_ECU

// ---------------------------------------------------------------------------
// Dashboard — receives and displays vehicle data
// ---------------------------------------------------------------------------

#ifdef ROLE_DASHBOARD

static uint16_t rpm     = 0;
static uint8_t  throttle = 0;
static uint16_t speed   = 0;
static int8_t   coolant = 0;
static bool     fanOn   = false;

static void printDash()
{
    Serial.printf("DASH  rpm=%4d  throttle=%3d%%  speed=%3d km/h  coolant=%3d C  fan=%s\n",
                  rpm, throttle, speed, coolant, fanOn ? "ON" : "off");
}

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 0, MODE_CLASSIC);

    Serial.println("\n==========================");
    Serial.println("  Classic CAN Dashboard");
    Serial.println("==========================");
    Serial.printf("configure: %s\n", s == CanStatus::OK ? "OK" : "FAIL");
    Serial.println("Waiting for vehicle data...\n");
}

void loop()
{
    CanMsg rx = {};
    if (!can.receive(rx)) return;

    if (rx.id == ID_ENGINE)
    {
        rpm      = ((uint16_t)rx.data[0] << 8) | rx.data[1];
        throttle = rx.data[2];
    }
    else if (rx.id == ID_SPEED)
    {
        speed = ((uint16_t)rx.data[0] << 8) | rx.data[1];
    }
    else if (rx.id == ID_COOLANT)
    {
        coolant = (int8_t)rx.data[0] - 40;  // undo offset encoding
        fanOn   = rx.data[1] != 0;
        printDash();  // print on the slow frame so output isn't flooded
    }
}

#endif  // ROLE_DASHBOARD
