# bus_monitor

Two ESP32 boards transmitting CAN FD frames continuously — a starting point for any two-node project.

## What you'll learn

How to run two nodes on the same bus, each with its own CAN ID, transmitting and receiving at a configurable rate.

## Hardware

Two ESP32 boards, each wired to an MCP2518FD and connected to the same CAN bus (CANH to CANH, CANL to CANL, shared ground).

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## Setup

Flash the `node_a` environment to one board and `node_b` to the other:

```
pio run -e node_a --target upload   # SID=0x100
pio run -e node_b --target upload   # SID=0x200
```

Both boards start transmitting immediately on boot — no serial input required.

## What to expect

Each board prints every frame it receives from the other. Press `+`/`-` to adjust the TX interval, `s` for a status summary.

```
==========================
  CAN FD Bus Monitor
==========================
SID=0x100  configure: OK
'+'/'-' TX interval  's' status
  RX ID=0x200 count=0
  RX ID=0x200 count=1
  RX ID=0x200 count=2
```
