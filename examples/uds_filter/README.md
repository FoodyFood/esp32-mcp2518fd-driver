# uds_filter

Filter incoming CAN FD frames by ID in hardware — demonstrated with a UDS diagnostic request/response pattern.

## What you'll learn

How to set an acceptance filter so your board only wakes up for the frames it cares about. On a busy bus with dozens of IDs flying past, the MCP2518FD discards everything that doesn't match your filter before it ever reaches your code.

## Hardware

Two ESP32 boards, each wired to an MCP2518FD and connected to the same CAN bus (CANH to CANH, CANL to CANL, shared ground).

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## Setup

Flash `tester` to one board and `ecu` to the other:

```
pio run -e tester --target upload
pio run -e ecu    --target upload
```

Open a Serial monitor (115200 baud) on each board.

## What to expect

The tester sends a ReadDataByIdentifier request every second. The ECU responds with a simulated engine temperature that slowly climbs. The tester prints only the response — all other traffic is discarded in hardware.

**Tester serial output:**
```
==========================
  UDS Tester
==========================
configure: OK
Filter: accept 0x7EC only
Sending ReadDataByIdentifier every 1 s...

TX request DID=0xF405
RX response  DID=0xF405  engine temp=23 C
TX request DID=0xF405
RX response  DID=0xF405  engine temp=24 C
```

**ECU serial output:**
```
==========================
  UDS ECU
==========================
configure: OK
Waiting for requests on 0x7E0...

  RX request DID=0xF405  responding with temp=23 C
  RX request DID=0xF405  responding with temp=24 C
```
