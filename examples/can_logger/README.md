# can_logger

Passively monitor a CAN FD bus — every frame captured with a hardware timestamp, completely invisible to other nodes.

## What you'll learn

How to put the MCP2518FD into listen-only mode so it receives every frame on the bus without transmitting anything — no ACK bits, no error frames. Other nodes cannot tell it is there. Every received frame is printed to Serial with a hardware timestamp accurate to 50 ns.

## Hardware

One ESP32 board wired to an MCP2518FD and connected to any active CAN FD bus running at 125 kbps nominal / 2 Mbps data.

To test without a live bus, run `scope_loopback` on a second board connected to the same bus — it transmits continuously at the matching rate.

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## Setup

Flash `can_logger` to one board. If using `scope_loopback` as a traffic source, flash it to a second board on the same bus.

```
pio run -e can_logger --target upload
```

Open a Serial monitor (115200 baud) on the logger board.

> **Two-board tip:** Flash the logger first, then the transmitter. After both are flashed, press reset on the transmitter first, then the logger — this ensures the logger is listening before traffic starts.

## What to expect

Every frame on the bus is printed as it arrives. Timestamps increment continuously from power-on.

```
==========================
  CAN FD Logger
==========================
configure: OK  FSYS: 20000000 Hz
Listening — all frames printed below

t=  1234.567 ms  ID=0x123       FD BRS  DLC=8  01 02 03 04 05 06 07 08
t=  1245.563 ms  ID=0x123       FD BRS  DLC=8  01 02 03 04 05 06 07 08
t=  1256.559 ms  ID=0x123       FD BRS  DLC=8  01 02 03 04 05 06 07 08
```

Extended IDs are marked with `(E)`:
```
t=  5000.000 ms  ID=0x1C42017B(E)  FD BRS  DLC=8  01 02 03 04 05 06 07 08
```
