# MCP2518FD CAN FD Driver for ESP32

A clean, auditable CAN FD driver for the **MCP2518FD** controller on **ESP32** over SPI.
No third-party CAN library. No undocumented assumptions. Every register address, bit position
and field definition is verified against the official Microchip datasheets before any code is written.

```cpp
MCP2518Driver can(spi, PIN_CS);
can.configure(500000, 2000000, MODE_NORMAL);  // 500 kbps nominal, 2 Mbps data — done

CanMsg tx = { .id=0x123, .fdf=true, .brs=true, .dlc=8 };
can.transmit(tx);

CanMsg rx;
can.receive(rx, 500);  // blocking, 500 ms timeout
```

---

## Use cases

| Use case | Description |
|---|---|
| **EV battery gateway** | Read cell voltages, SOC and temperatures from a CAN FD BMS (Kia 64 FD, VW MEB) and re-publish over a second bus or WiFi |
| **UDS diagnostics** | Send ISO 14229 requests to a vehicle ECU over CAN FD and receive multi-frame ISO-TP responses |
| **Inverter interface** | Send torque/speed setpoints to a CAN FD motor controller and read back telemetry at 10 ms intervals |
| **CAN FD data logger** | Capture every frame on a bus with per-frame timestamps, stream in candump format over USB Serial |
| **Peer-to-peer telemetry** | Two ESP32 boards talking directly over CAN FD — sensor nodes, drone ESCs, data concentrators |
| **Scope / analyser stimulus** | Drive known CAN FD frames onto the bus for oscilloscope or protocol analyser capture |
| **Production self-test** | Verify chip and transceiver wiring at factory or field bring-up — no second node required |

See [`docs/use_case_coverage.md`](docs/use_case_coverage.md) for the full feature-by-use-case coverage matrix and gap tracking.

---

## Features

| Feature | Status |
|---|---|
| Auto-detect oscillator frequency (20 MHz / 40 MHz) from OSC register | ✅ |
| Calculate all bit timing registers from target rates — no presets required | ✅ |
| Transmitter delay compensation (TDC) auto-configured at ≥ 1 Mbps | ✅ |
| Transmit CAN FD frames up to 64 bytes (DLC 0–15) | ✅ |
| Receive CAN FD frames, non-blocking and blocking with timeout | ✅ |
| Data rates 1 / 2 / 4 / 5 Mbps at 20 MHz; up to 8 Mbps at 40 MHz | ✅ |
| Runtime data rate switch without losing nominal configuration | ✅ |
| Internal loopback mode (no bus required) | ✅ |
| External loopback mode (real bus signals, self-ACK) | ✅ |
| Listen-only mode (passive, no ACK) | ✅ |
| Two-node normal mode — verified on real hardware | ✅ |
| Raw / advanced API for non-standard rates and custom oscillators | ✅ |
| 29-bit extended ID (EID) — 11-bit and 29-bit on the same bus | ✅ |
| Acceptance filter API — per-SID, per-range, per-mask | ✅ |
| Bus error detection — TEC/REC counters, bus-off flag | 🔜 [SPEC-003](docs/specs/SPEC-003-bus-error-and-tx-result.md) |
| TX error detail — distinguish no-ACK, bus error, FIFO full | 🔜 [SPEC-003](docs/specs/SPEC-003-bus-error-and-tx-result.md) |
| Interrupt-driven RX via INT pin (GPIO 34) | 🔜 [SPEC-004](docs/specs/SPEC-004-interrupt-rx-and-fifo-depth.md) |
| Configurable RX FIFO depth (1–32 slots) | 🔜 [SPEC-004](docs/specs/SPEC-004-interrupt-rx-and-fifo-depth.md) |
| Per-frame RX timestamp from hardware time base counter | 🔜 [SPEC-005](docs/specs/SPEC-005-rx-timestamp-and-listen-only.md) |
| stop() / restart() / sleep() lifecycle control | 🔜 [SPEC-006](docs/specs/SPEC-006-stop-restart-sleep.md) |

---

## Why this driver

Every existing Arduino/ESP32 CAN FD library for the MCP2518FD either wraps Microchip's own
`canfdspi` API or makes undocumented assumptions about register state. This driver is built
from scratch, one register at a time, with a clear paper trail from datasheet to working
hardware for every decision.

- **Auditable** — every register write is traceable to a datasheet page and table number
- **Minimal** — no RTOS, no heap allocation, no dependencies beyond Arduino SPI
- **Verified** — every feature is tested on two real hardware nodes before being committed;
  no feature is marked done without hardware evidence from both nodes. Bus signals are
  verified with a DSO at each stage — not just software assertions
- **Spec-driven** — new features start as a written spec with acceptance criteria; code
  follows the spec, not the other way around

---

## Test Hardware

This driver was developed and verified on the following hardware. Other ESP32 boards and MCP2518FD breakout variants should work provided the SPI pins are configured correctly.

<img src="docs/images/mcp2518fd_breakout_board.jpg" width="400" alt="MCP2518FD breakout board">

| Item | Detail |
|---|---|
| MCU | ESP32-D0WD-V3 (rev 3.1) |
| CAN controller | MCP2518FD |
| Oscillator | 20 MHz crystal (SCLKDIV=0, PLLEN=0 → FSYS=20 MHz) |
| Transceiver | ATA6561 |
| SPI bus | VSPI — SCK=33, MISO=35, MOSI=32, CS=25 |
| INT | GPIO 34 (interrupt input — used by SPEC-004) |

---

## Quick start

```cpp
#include "mcp2518fd_can.h"

SPIClass      spi(VSPI);
MCP2518Driver can(spi, PIN_CS);

void setup() {
    spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    CanStatus s = can.configure(500000, 2000000, MODE_NORMAL);
    if (s != CanStatus::OK) { /* handle error */ }
}

void loop() {
    // Transmit
    CanMsg tx = { .id=0x123, .fdf=true, .brs=true, .dlc=8 };
    for (int i = 0; i < 8; i++) tx.data[i] = i;
    can.transmit(tx);

    // Non-blocking receive
    CanMsg rx;
    if (can.available()) can.receive(rx);

    // Blocking receive with timeout
    can.receive(rx, 500);

    // Switch data rate at runtime
    can.setDataRate(4000000);  // 4 Mbps

    // Detected oscillator frequency
    Serial.printf("FSYS: %lu Hz\n", can.getFsys());
}
```

### CanStatus

| Value | Meaning |
|---|---|
| `CanStatus::OK` | Success |
| `CanStatus::MODE_TIMEOUT` | Chip did not confirm the requested mode |
| `CanStatus::RATE_NOT_ACHIEVABLE` | Target rate cannot be reached at the detected FSYS |
| `CanStatus::CLOCK_NOT_READY` | OSC register shows clock not stable after reset |

### Supported rates

| Nominal | Data | 20 MHz | 40 MHz |
|---|---|---|---|
| 125 kbps | 1–5 Mbps | ✅ | ✅ |
| 250 kbps | 1–5 Mbps | ✅ | ✅ |
| 500 kbps | 1–5 Mbps | ✅ | ✅ |
| 1 Mbps | 1–5 Mbps | ✅ | ✅ |
| any | 8 Mbps | ❌ | ✅ |

### Raw / advanced API

For direct register control — non-standard rates, custom oscillators:

```cpp
can.configureRaw(NBTCFG_125K_40MHZ, DBTCFG_2M_40MHZ, TDC_2M_40MHZ, MODE_NORMAL);
can.setDataBitTimingRaw(DBTCFG_8M_40MHZ, TDC_8M_40MHZ);
```

Presets for 20 MHz and 40 MHz are defined in `mcp2518fd_presets.h`. All use BRP=0, exact rates, 80% sample point.

---

## Examples

Each example is a self-contained PlatformIO project.

| Example | Description |
|---|---|
| `examples/single_node` | Single-board regression test — configure(), bitrates, raw API. No bus required. |
| `examples/id_filter` | Acceptance filter demonstration — SID exact, range, EID, multi-filter, catch-all. |
| `examples/two_node` | Two-board bidirectional test over a real CAN bus — A→B and B→A at 2/4/5 Mbps, 8-byte and 64-byte payloads. |
| `examples/walkie_talkie` | Text chat between two boards over CAN FD — type in one Serial monitor, read on the other. |
| `examples/scope_loopback` | Continuous TX in `MODE_EXTERNAL_LB` for oscilloscope measurements — real bus signals, self-ACK. |
| `examples/bus_monitor` | Two nodes continuously transmitting counters — autonomous boot, no serial input required. node_a env → COM4 (ID=0x100), node_b env → COM3 (ID=0x200). Bus load testing and integrity checking. |

```bash
# Full regression suite (upload + verify all three automatable examples):
python tests/integration/verify.py --suite all --port COM4 --port-b COM3

# Single suite:
python tests/integration/verify.py --suite single_node --port COM4
python tests/integration/verify.py --suite id_filter   --port COM4
python tests/integration/verify.py --suite two_node    --port COM4 --port-b COM3

# Unit tests (no hardware required):
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
```

---

## System overview

```mermaid
graph LR
    subgraph ESP32["🔵 ESP32-D0WD-V3"]
        APP["Application\nmain.cpp"]
        CAN["CAN Driver\nmcp2518fd_can"]
        DRV["SPI Transport\nmcp2518fd_spi"]
    end

    subgraph SPI["⚡ SPI Bus (VSPI 1MHz)"]
        direction LR
        SCK["SCK GPIO33"]
        MOSI["MOSI GPIO32"]
        MISO["MISO GPIO35"]
        CS["CS GPIO25"]
    end

    subgraph MCP["🟠 MCP2518FD"]
        CTRL["CAN FD\nController"]
        RAM["Message RAM\n2KB"]
        FIFO1["FIFO1 TX\n0x400"]
        FIFO2["FIFO2 RX\n0x410"]
    end

    subgraph BUS["🟢 CAN Bus"]
        TRX["ATA6561\nTransceiver"]
        CANH["CANH"]
        CANL["CANL"]
    end

    APP --> CAN
    CAN --> DRV
    DRV <--> SPI
    SPI <--> CTRL
    CTRL <--> RAM
    RAM --- FIFO1
    RAM --- FIFO2
    CTRL <--> TRX
    TRX --- CANH
    TRX --- CANL

    style ESP32 fill:#1a3a5c,stroke:#4a9eff,color:#fff
    style MCP fill:#7a3a00,stroke:#ff8c00,color:#fff
    style BUS fill:#1a4a1a,stroke:#4aff4a,color:#fff
    style SPI fill:#2a2a2a,stroke:#888,color:#fff
```

---

## Repository layout

```
include/
  mcp2518fd_can.h           # Public driver API — CanMsg, MCP2518Driver
  mcp2518fd_presets.h       # Bit timing preset constants (Arduino-free)
  mcp2518fd_timing.h        # Pure-logic timing functions — calcBitTiming, calcTxTimeout, EID/filter encode
  mcp2518fd_spi.h           # SPI transport layer
  mcp2518fd_registers.h     # All register addresses, masks and constants

src/
  mcp2518fd_can.cpp         # CAN driver implementation
  mcp2518fd_spi.cpp         # SPI transport implementation

examples/
  single_node/              # Single-board config and bitrate regression tests
  id_filter/                # Acceptance filter demonstration (SID/EID filtering)
  two_node/                 # Two-node CAN FD test — bidirectional over real bus
  walkie_talkie/            # Text chat between two nodes over CAN FD
  scope_loopback/           # Continuous TX in MODE_EXTERNAL_LB for scope measurements
  bus_monitor/              # Two nodes continuously talking — bus load + integrity check

tests/
  integration/
    verify.py               # Entry point — upload + verify, single suite or all
    mcp_test/               # runner, suites, upload, serial I/O modules
  unit/
    platformio.ini          # Native PlatformIO env — runs on host, no hardware required
    test/test_unit/
      test_main.cpp         # 50 unit tests: dlcToLen, calcBitTiming, calcTxTimeout, EID/filter encode, register addresses

tools/
  search.py                 # PDF search tool — queries both datasheets

docs/
  status.md                 # Verified milestone tracker
  context.md                # Hardware decisions and discoveries
  registers.md              # Register field reference
  use_case_coverage.md      # Real-world use case analysis and gap tracking
  specs/                    # One spec per feature — read before implementing
    README.md               # Spec index and implementation order
    SPEC-NNN-*.md           # Individual feature specs
  reference/                # Place downloaded PDFs here (see reference/README.md)

.github/
  workflows/
    ci.yml                  # CI: unit tests + build all examples on every PR, auto-merge on pass

library.json                # PlatformIO library manifest
library.properties          # Arduino IDE library manifest
```

---

## Source of truth

All register addresses, bit positions and field definitions are verified against:

| Document | ID | Link |
|---|---|---|
| MCP2518FD Datasheet | DS20006027B | https://www.microchip.com/en-us/product/MCP2518FD |
| MCP25XXFD Family Reference Manual | DS20005678E | https://www.microchip.com/en-us/product/MCP2518FD |

PDFs are not committed to this repo. Download them and place in `docs/reference/` — see [`docs/reference/README.md`](docs/reference/README.md).

---

## CI

Every PR runs unit tests and builds all examples on `ubuntu-24.04`. Auto-merges on pass.

[![CI](https://github.com/FoodyFood/esp32-mcp2518fd-driver/actions/workflows/ci.yml/badge.svg)](https://github.com/FoodyFood/esp32-mcp2518fd-driver/actions/workflows/ci.yml)

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) 6.x
- Espressif32 platform 7.0.1

Optional (integration test runner and PDF search tool):
```bash
pip install -r requirements.txt
```

## License

MIT
