# classic_can

Use the MCP2518FD as a plain CAN 2.0B controller — compatible with any legacy CAN device.

## What you'll learn

How to configure the MCP2518FD in Classic CAN mode so it communicates with devices that don't support CAN FD. The chip transmits and receives standard 8-byte frames at a fixed bit rate with no bit rate switching. Any classic CAN device on the bus will see it as a normal CAN node.

The example simulates a simple vehicle: one board acts as a powertrain ECU broadcasting RPM, speed and coolant temperature; the other acts as a dashboard reading and displaying those values.

## Hardware

Two ESP32 boards, each wired to an MCP2518FD and connected to the same CAN bus (CANH to CANH, CANL to CANL, shared ground).

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## Setup

Flash `ecu` to one board and `dashboard` to the other:

```
pio run -e ecu       --target upload
pio run -e dashboard --target upload
```

Open a Serial monitor (115200 baud) on each board.

## What to expect

The ECU simulates an engine slowly accelerating and warming up. The dashboard prints a live readout every 100 ms.

**ECU serial output:**
```
==========================
  Classic CAN ECU
==========================
configure: OK
Broadcasting engine, speed and temperature...

ECU  rpm= 800  speed=  0 km/h  coolant= 20 C  fan=off
ECU  rpm=1200  speed= 10 km/h  coolant= 21 C  fan=off
ECU  rpm=2000  speed= 50 km/h  coolant= 45 C  fan=off
ECU  rpm=3000  speed=100 km/h  coolant= 90 C  fan=ON
```

**Dashboard serial output:**
```
==========================
  Classic CAN Dashboard
==========================
configure: OK
Waiting for vehicle data...

DASH  rpm= 800  throttle=  0%  speed=  0 km/h  coolant= 20 C  fan=off
DASH  rpm=1200  throttle= 18%  speed= 10 km/h  coolant= 21 C  fan=off
DASH  rpm=3000  throttle=100%  speed=100 km/h  coolant= 90 C  fan=ON
```
