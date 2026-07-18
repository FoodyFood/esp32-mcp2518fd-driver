# Project Context

## Hardware

| Item         | Detail                        |
|--------------|-------------------------------|
| MCU          | ESP32-D0WD-V3 (rev 3.1)       |
| CAN chip     | MCP2518FD                     |
| Oscillator   | 20 MHz crystal (OSC reg: SCLKDIV=0, PLLEN=0 → FSYS=20 MHz) |
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
- No TEF
- No TXQ
- One TX FIFO, one RX FIFO
- Minimal code — only what is needed for the current milestone

## Architecture

| File                        | Purpose                                                      |
|-----------------------------|--------------------------------------------------------------|
| `include/mcp2518fd_can.h`   | MCP2518Driver public API, CanMsg struct, bit timing presets  |
| `include/mcp2518fd_spi.h`   | MCP2518SPI class declaration                                 |
| `include/mcp2518fd_registers.h` | Register addresses, masks, constants                     |
| `src/mcp2518fd_can.cpp`     | CAN driver — configure, transmit, receive, bitrate switch    |
| `src/mcp2518fd_spi.cpp`     | SPI transport + mode control implementation                  |
| `examples/loopback/`        | Regression test — single-board internal loopback             |
| `examples/two_node/`        | Two-node bidirectional test over real bus                    |
| `examples/walkie_talkie/`   | Text chat between two nodes                                  |
| `examples/scope_loopback/`  | Continuous TX in MODE_EXTERNAL_LB for scope measurements     |
| `examples/bus_monitor/`     | Two nodes continuously talking — bus load + integrity check. Autonomous: node_a env → COM4 (SID=0x100), node_b env → COM3 (SID=0x200) |
| `docs/context.md`           | This file                                                    |
| `docs/status.md`            | Verified milestone tracker                                   |
| `docs/registers.md`         | Register field reference                                     |
| `tools/run_test.py`         | Automated test runner — loopback and two-node                |
| `tools/check_timing.py`     | Verify bit timing preset values against datasheet formula    |
| `tools/find_timing.py`      | Calculate NBTCFG/DBTCFG values for a target rate             |
| `docs/search.py`            | PDF search tool — queries both reference PDFs                |

## Key Decisions

- `configure(nominalBps, dataBps, mode)` is the primary API. It reads the OSC register after reset, derives FSYS, and calculates NBTCFG/DBTCFG/TDC from first principles. No preset knowledge required from the caller.
- `configureRaw()` and `setDataBitTimingRaw()` exist for direct register access (non-standard rates, custom oscillators). Presets in the header are for this path only.
- `setDataRate()` calculates timing before entering config mode. A `RATE_NOT_ACHIEVABLE` result leaves the chip in its current mode completely untouched.
- `setMode()` writes only byte 3 of CiCON (REQOP bits 2:0) via `write8`.
  A 32-bit read-modify-write of CiCON was found to be unreliable on this chip.
- `getMode()` reads byte 2 of CiCON and extracts OPMOD from bits 7:5.
- All multi-byte reads/writes are little-endian (LSB first), matching MCP2518FD SPI byte order.
- SPI transaction is opened once in `begin()` and left open (no per-transfer beginTransaction/endTransaction).
- FIFO1 = TX, FIFO2 = RX. TXQ and TEF not used.
- Acceptance filter 0 configured as catch-all (SID/mask all zeros) pointing to FIFO2.
- TDC (Transmitter Delay Compensation) required at data rates >= 1 Mbps. Use TDCMOD=auto, TDCO=(BRP+1)*(TSEG1+1).

## Discoveries

### CiCON 32-bit RMW is broken
A 32-bit read-modify-write of CiCON does not reliably change the operating mode.
Solution: write only byte 3 of CiCON (contains REQOP[2:0]) using `write8(REG_CiCON + 3, value)`.
This is consistent with how ACAN2517FD handles it (verified via Issue #43).

### CiTXQCON.TXEN always reads 1
Per datasheet Register 3-26: TXEN in CiTXQCON is hardwired — "This bit always reads as 1".
Observed on hardware: CiTXQCON = 0x00600080, TXEN bit (bit 7) = 1 at all times. Expected.

### FRESET behaviour
FRESET (bit 10 of CiFIFOCONm) is set automatically when entering Configuration mode
and cleared automatically when leaving it. Do not poll or set it manually while in config mode.
Observed: FRESET=0 when read in loopback mode (after config mode exit). Expected.

### Physical bus output confirmed (MODE_EXTERNAL_LB)
With `MODE_EXTERNAL_LB`, the MCP2518FD drives real differential signals on CANH/CANL via the ATA6561
transceiver while ACKing its own frames internally. Clean waveforms observed on oscilloscope at
125 kbps nominal / 2 Mbps data, SID=0x123 DLC=8, 10ms frame interval.
This confirms the transceiver wiring is correct and the physical bus layer is ready for two-node.

### Oscillator is 20 MHz, not 40 MHz
The OSC register (0xE00) after reset reads 0x00000460: SCLKDIV=0 (bit 4), PLLEN=0 (bit 0), OSCREADY=1 (bit 10).
This means FSYS = 20 MHz directly from the crystal with no PLL and no divider.
All timing presets and the auto-calculation in detectFsys() are based on 20 MHz for this hardware.
Scope verification: 125 kbps nominal → first dominant run = 24 µs = 3 bits × 8 µs/bit. Correct.
8 Mbps data rate is not achievable at 20 MHz (20M/8M = 2.5 TQ, non-integer). RATE_NOT_ACHIEVABLE returned.

### setDataRate() must calculate before touching the chip
Early implementation entered config mode before checking if the rate was achievable, leaving the chip
in config mode on failure. Fixed: calcBitTiming() is called first; only if it succeeds does the driver
enter config mode. A RATE_NOT_ACHIEVABLE result is now completely non-destructive.
At data bit rates of 1 Mbps and above the chip cannot reliably sample the loopback signal without
Transmitter Delay Compensation. Symptoms: TX completes (TXREQ clears, no errors) but FIFO2
remains empty. Fix: set TDCMOD=2 (auto) and TDCO=(BRP+1)*(TSEG1+1) in CiTDC before
entering the active mode. Verified at 2 Mbps with TDCO=19.

### CiFIFOUAm reports offset, not absolute address
CiFIFOUAm holds the byte offset from RAM base (0x400), not the absolute SPI address.
Actual RAM address = 0x400 + UA.
Verified: UA1=0x000 → RAM 0x400, UA2=0x010 → RAM 0x410 (16-byte objects, PLSIZE=0, no TEF/TXQ).
