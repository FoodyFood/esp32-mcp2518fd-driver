# walkie_talkie

Text chat between two ESP32 boards over CAN FD.

## What you'll learn

How to transmit and receive variable-length messages over CAN FD using the same binary on both nodes.

## Hardware

Two ESP32 boards, each wired to an MCP2518FD and connected to the same CAN bus (CANH to CANH, CANL to CANL, shared ground).

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## Setup

Flash this same binary to both boards — no configuration needed, both nodes are identical.

Open a Serial monitor (115200 baud) on each board.

## What to expect

Type a message on one board and press Enter. It appears on the other board's Serial monitor prefixed with `THEM:`. Both boards can send and receive simultaneously.

```
==========================
  CAN FD Walkie-Talkie
==========================
configure: OK
Type a message and press Enter to send.

ME: hello from node A
THEM: hello back from node B
```
