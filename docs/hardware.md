# Hardware Setup

## Verified hardware

| Board | MCP2518FD | Oscillator | Transceiver | Notes |
|---|---|---|---|---|
| Custom breakout (ESP32-D0WD-V3) | External | 20 MHz | ATA6561 | All examples and integration tests |
| LilyGo T2-CANFD | Integrated | 40 MHz | Integrated | Full test suite — all tests pass |

The driver works with any board that pairs an ESP32 with an MCP2518FD over SPI, whether the chip is on a separate breakout or integrated on the same PCB. Configure the `PIN_*` constants to match your wiring and pass `fsysHz` to `configure()` if your oscillator is 40 MHz with PLL disabled.

## Default pin assignment

| Signal | GPIO |
|---|---|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |
| INT | 34 (optional) |

These are the pins used in all examples. Change the `PIN_*` constants at the top of any example to match your wiring.

## SPI configuration

```cpp
SPIClass spi(VSPI);
spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
```

The driver runs the SPI bus at 1 MHz. The MCP2518FD supports up to 85 MHz but 1 MHz is conservative and reliable across all cable lengths and breadboard setups.

## INT pin (optional)

Connect the MCP2518FD INT pin to any free GPIO and pass it to the constructor:

```cpp
MCP2518Driver can(spi, PIN_CS, PIN_INT);
```

Without the INT pin the driver polls for received frames over SPI. With it, the chip pulls INT low the moment a frame arrives and the driver sets a flag in the ISR — `available()` returns true immediately with no SPI transaction. See the `int_pin` example.

## Oscillator auto-detection

The driver reads the OSC register after reset and derives the system clock frequency automatically. Both 20 MHz and 40 MHz crystals are supported. Call `getFsys()` after `configure()` to confirm what was detected.

## Bus wiring

Connect CANH to CANH and CANL to CANL between all nodes. Add 120 Ω termination resistors at each end of the bus. Share ground between all nodes.

For a two-board bench setup a short wire pair with one terminator at each end is sufficient.
