# can_logger

Passively monitor a CAN bus with per-frame hardware timestamps — completely invisible to other nodes.

## What you'll learn

How to put the MCP2518FD into listen-only mode so it captures every frame without transmitting ACK bits or error frames. Each received frame carries a hardware timestamp accurate to 50 ns, captured at the start of frame.

## Hardware

One ESP32 board wired to an MCP2518FD, connected to any active CAN bus.

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

Connect CANH and CANL to the bus you want to monitor. No termination resistor is needed if the bus already has one at each end.

## Setup

Flash the single `can_logger` environment:

```
pio run -e can_logger --target upload
```

Open a Serial monitor at 115200 baud. Frames appear as they arrive.

To test without a live bus, change `MODE_LISTEN` to `MODE_INTERNAL_LB` in `setup()` and add a transmit call in `loop()`.

## What to expect

Every frame on the bus is printed in candump-style format with a hardware timestamp:

```
==========================
  CAN FD Logger
==========================
configure: OK  FSYS: 20000000 Hz
Listening — all frames printed below

t=  1250.700 ms  ID=0x100       FD BRS  DLC=8  01 02 03 04 05 06 07 08
t=  1300.750 ms  ID=0x200       FD BRS  DLC=8  00 00 00 01 00 00 00 00
t=  1350.800 ms  ID=0x18DAF110(E)  FD BRS  DLC=8  03 22 F4 05 00 00 00 00
```

The timestamp is the raw TBC counter value converted to milliseconds. The counter runs from power-on and wraps after ~214 seconds.
