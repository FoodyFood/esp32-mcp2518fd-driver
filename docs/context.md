# Project Context

## Hardware

| Item         | Detail                        |
|--------------|-------------------------------|
| MCU          | ESP32-D0WD-V3 (rev 3.1)       |
| CAN chip     | MCP2518FD                     |
| Oscillator   | 40 MHz crystal                |
| SPI bus      | VSPI                          |
| SCK          | GPIO 33                       |
| MISO         | GPIO 35                       |
| MOSI         | GPIO 32                       |
| CS           | GPIO 25                       |
| INT          | GPIO 34 (unused so far)       |
| SPI speed    | 1 MHz                         |
| Framework    | Arduino (PlatformIO)          |
| Upload port  | COM4                          |

## Goal

Direct register-level driver for the MCP2518FD.
No third-party CAN library (ACAN2517FD removed).
Build the driver incrementally, one verified feature at a time.

## Constraints

- No ACAN library
- No interrupts (polling only, for now)
- No filters
- No TEF
- No TXQ
- No CAN bus (Internal Loopback only, for now)
- One TX FIFO, one RX FIFO
- Minimal code — only what is needed for the current milestone

## Architecture

| File                  | Purpose                                      |
|-----------------------|----------------------------------------------|
| src/main.cpp          | Test harness — interactive, key-triggered    |
| src/mcp2518fd_spi.h   | MCP2518SPI class declaration                 |
| src/mcp2518fd_spi.cpp | SPI transport + mode control implementation  |
| src/mcp2518fd_registers.h | Register addresses, masks, constants    |
| docs/context.md       | This file                                    |
| docs/status.md        | Verified milestone tracker                   |
| docs/registers.md     | Register field reference                     |

## Key Decisions

- `setMode()` writes only byte 3 of CiCON (REQOP bits 2:0) via `write8`.
  A 32-bit read-modify-write of CiCON was found to be unreliable on this chip.
- `getMode()` reads byte 2 of CiCON and extracts OPMOD from bits 7:5.
- All multi-byte reads/writes are little-endian (LSB first), matching MCP2518FD SPI byte order.
- SPI transaction is opened once in `begin()` and left open (no per-transfer beginTransaction/endTransaction).
