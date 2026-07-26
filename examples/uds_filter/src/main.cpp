#include <Arduino.h>
#include <SPI.h>

#include "mcp2518fd_can.h"

// Learning objective: use an acceptance filter so your board only wakes up for
// the frames it cares about — demonstrated with a UDS diagnostic request/response
// pattern. The tester sends requests on 0x7E0 and filters to receive only 0x7EC.
// Every other ID on the bus is silently discarded in hardware before it reaches
// your code.
//
// Flash tester to one board and ecu to the other (see platformio.ini).
// Open a Serial monitor on each at 115200 baud.
//
// The tester sends a ReadDataByIdentifier request every second.
// The ECU responds with a simulated engine temperature value.
// The tester prints only the response — all other traffic is filtered out.

constexpr uint8_t PIN_SCK  = 33;
constexpr uint8_t PIN_MISO = 35;
constexpr uint8_t PIN_MOSI = 32;
constexpr uint8_t PIN_CS   = 25;

// UDS IDs — standard OBD/UDS convention
constexpr uint32_t ID_TESTER_REQUEST  = 0x7E0;  // tester → ECU
constexpr uint32_t ID_ECU_RESPONSE    = 0x7EC;  // ECU → tester

// UDS service bytes
constexpr uint8_t SVC_READ_DATA_BY_ID          = 0x22;
constexpr uint8_t SVC_READ_DATA_BY_ID_RESPONSE = 0x62;
constexpr uint16_t DID_ENGINE_TEMP             = 0xF405;

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

// ---------------------------------------------------------------------------
// ECU — responds to ReadDataByIdentifier requests with a simulated temperature
// ---------------------------------------------------------------------------

#ifdef ROLE_ECU

static uint8_t fakeTemp = 20;  // degrees C, climbs over time

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 2000000, MODE_NORMAL);

    Serial.println("\n==========================");
    Serial.println("  UDS ECU");
    Serial.println("==========================");
    Serial.printf("configure: %s\n", s == CanStatus::OK ? "OK" : "FAIL");
    Serial.println("Waiting for requests on 0x7E0...\n");
}

void loop()
{
    // Slowly warm up the simulated engine
    static uint32_t lastWarm = 0;
    if (millis() - lastWarm > 3000) { lastWarm = millis(); if (fakeTemp < 95) fakeTemp++; }

    CanMsg rx = {};
    if (!can.receive(rx)) return;
    if (rx.id != ID_TESTER_REQUEST) return;

    // Check for ReadDataByIdentifier request for DID_ENGINE_TEMP
    if (rx.dlc < 3) return;
    if (rx.data[0] != SVC_READ_DATA_BY_ID) return;
    uint16_t did = ((uint16_t)rx.data[1] << 8) | rx.data[2];
    if (did != DID_ENGINE_TEMP) return;

    Serial.printf("  RX request DID=0x%04X  responding with temp=%d C\n", did, fakeTemp);

    // Positive response: 0x62 + DID + value
    CanMsg tx;
    tx.id     = ID_ECU_RESPONSE;
    tx.fdf    = true;
    tx.brs    = true;
    tx.dlc    = 4;
    tx.data[0] = SVC_READ_DATA_BY_ID_RESPONSE;
    tx.data[1] = (DID_ENGINE_TEMP >> 8) & 0xFF;
    tx.data[2] =  DID_ENGINE_TEMP       & 0xFF;
    tx.data[3] = fakeTemp;

    if (can.transmit(tx) != CanTxResult::OK)
        Serial.println("  [TX failed]");
}

#endif  // ROLE_ECU

// ---------------------------------------------------------------------------
// Tester — sends requests and filters to receive only ECU responses
// ---------------------------------------------------------------------------

#ifdef ROLE_TESTER

void setup()
{
    Serial.begin(115200);
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 2000000, MODE_NORMAL);

    // Accept only 0x7EC — the ECU response ID.
    // Any other frame on the bus is discarded before it reaches receive().
    can.setFilter(0, ID_ECU_RESPONSE, 0x7FF, false);

    Serial.println("\n==========================");
    Serial.println("  UDS Tester");
    Serial.println("==========================");
    Serial.printf("configure: %s\n", s == CanStatus::OK ? "OK" : "FAIL");
    Serial.println("Filter: accept 0x7EC only");
    Serial.println("Sending ReadDataByIdentifier every 1 s...\n");
}

void loop()
{
    static uint32_t lastRequest = 0;

    if (millis() - lastRequest >= 1000)
    {
        lastRequest = millis();

        // ReadDataByIdentifier request for engine temperature
        CanMsg tx;
        tx.id     = ID_TESTER_REQUEST;
        tx.fdf    = true;
        tx.brs    = true;
        tx.dlc    = 3;
        tx.data[0] = SVC_READ_DATA_BY_ID;
        tx.data[1] = (DID_ENGINE_TEMP >> 8) & 0xFF;
        tx.data[2] =  DID_ENGINE_TEMP       & 0xFF;

        Serial.printf("TX request DID=0x%04X\n", DID_ENGINE_TEMP);
        if (can.transmit(tx) != CanTxResult::OK)
            Serial.println("  [TX failed]");
    }

    // Only 0x7EC frames reach here — everything else was filtered in hardware
    CanMsg rx = {};
    if (can.receive(rx))
    {
        uint16_t did  = ((uint16_t)rx.data[1] << 8) | rx.data[2];
        uint8_t  temp = rx.data[3];
        Serial.printf("RX response  DID=0x%04X  engine temp=%d C\n", did, temp);
    }
}

#endif  // ROLE_TESTER
