# int_pin

Interrupt-driven RX — the main loop stays free while CAN FD frames arrive.

## What you'll learn

How to wire the MCP2518FD INT pin so frame arrival triggers an ISR instead of polling.
Without the INT pin, `available()` reads a register over SPI on every call — you either
poll constantly or add delays and miss frames. With the INT pin the driver sets a flag the
moment a frame lands; `available()` returns true immediately with no SPI transaction.

## Hardware

Single board — no second node required. The example runs in internal loopback.

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |
| INT | 34 |

Connect the MCP2518FD INT pin to GPIO 34.

## What to expect

A frame is transmitted every 100 ms. The Serial monitor prints loop iterations and RX rate
every 2 s. A high loops/frame value confirms the main loop is never blocked waiting for frames.

```
configure: OK  FSYS: 20000000 Hz
Running — loop count and RX rate printed every 2 s

frames rx:   20   loop iterations:  186432   loops/frame: ~9321
frames rx:   40   loop iterations:  373011   loops/frame: ~9325
```
