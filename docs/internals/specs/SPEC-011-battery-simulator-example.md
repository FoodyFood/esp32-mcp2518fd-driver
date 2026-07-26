# SPEC-011 — VW MEB Battery Simulator Example

## Status
Pending

## Dependencies
- SPEC-001 (29-bit extended ID) — Done — required for MEB's 29-bit frame IDs
- SPEC-002 (acceptance filters) — Done — required to filter BMS_20 by ID
- SPEC-003 (bus error detection + TX error detail) — Done — required for TX feedback
- SPEC-008 (Classic CAN mode) — Done — required for mixed FD/classic frames (NMH_*)

## Use case
UC-8 — CAN FD Battery Simulator (VW MEB)

## Goal

Create `examples/meb_battery_sim/` — a self-contained PlatformIO example that
impersonates a VW MEB battery on a CAN FD bus.

A second node running Battery-Emulator (or any device that speaks the MEB protocol)
connects to the same bus and will see a healthy, contactors-closed battery. The example
demonstrates the full MEB keepalive schedule, VAG CRC, rolling counters, mixed FD and
classic CAN frames, and 29-bit extended IDs — all using only the public driver API.

This is the most real-world example in the library. Every frame ID, payload, CRC
constant and timing interval is sourced directly from
`Battery-Emulator/Software/src/battery/MEB-BATTERY.cpp` and `.h`. Nothing is invented.

## What "healthy battery" means to the MEB BMS

The Battery-Emulator node transmits keepalive frames and watches BMS_20 (0xCF) from
the battery. BMS_20 carries `BMS_mode` in bits [2:0] of byte 2:

| BMS_mode | Meaning |
|---|---|
| 7 | Init — battery not ready |
| 0 | Standby — ready, contactors open |
| 1 | HV_ACTIVE — contactors closed, normal operation |
| 3 | External charging |
| 4 | AC charging |
| 5 | Error |

The Battery-Emulator node closes contactors when it sees BMS_mode = 1 (HV_ACTIVE).
Our simulator must transmit BMS_20 with BMS_mode = 1 to signal that the battery is
healthy and the contactors are closed.

The contactor-closing sequence also requires the following frames to be present on the
bus before the Battery-Emulator node will request HV:
- ESC_51_Auth (0xFC) — 10 ms, 48-byte FD frame with rolling counter and VAG CRC
- ESP_21 (0xFD) — 20 ms, 8-byte FD frame with rolling counter and VAG CRC
- Airbag_01 (0x40) — 40 ms, 8-byte FD frame with rolling counter and VAG CRC
- EM1_01 (0xC0) — 50 ms, 32-byte FD frame with rolling counter and VAG CRC
- HVK_01 (0x503) — 100 ms, 8-byte FD frame with rolling counter and VAG CRC

These are the frames Battery-Emulator transmits *to* the battery. Our simulator
transmits BMS_20 *from* the battery. The two nodes talk to each other.

## Bus parameters
- Nominal: 500 kbps
- Data: 2 Mbps
- Mode: MODE_NORMAL

## Frame schedule

All frame IDs, DLCs, payloads, CRC constants and intervals are taken directly from
`MEB-BATTERY.h` and `MEB-BATTERY.cpp`. The simulator transmits the frames that the
real MEB battery transmits — i.e. the frames that Battery-Emulator *receives*.

| Frame | ID | DLC | FD | EID | Interval | CRC |
|---|---|---|---|---|---|---|
| BMS_20 | 0xCF | 8 | yes | no | 10 ms | VAG (BMS_20_PDU_CONST) |
| BMS_22 | 0x12DD54D1 | 8 | yes | yes | 100 ms | no |
| BMS_21 | 0x12DD54D0 | 8 | yes | yes | 100 ms | no |
| BMS_04 | 0x5A2 | 8 | yes | no | 100 ms | VAG (BMS_04_PDU_CONST) |
| BMS_07 | 0x5CA | 8 | yes | no | 500 ms | VAG (BMS_07_PDU_CONST) |
| KN_Hybrid_01 | 0x17F0007B | 8 | yes | yes | 500 ms | no |
| NMH_Hybrid_01 | 0x1B00007B | 8 | classic | yes | 200 ms | no |

BMS_20 is the most critical frame. It must be transmitted at 10 ms with BMS_mode = 1
(HV_ACTIVE) and a correct VAG CRC. Without it, the Battery-Emulator node will not
close contactors.

BMS_22 carries SOC and usable energy. BMS_21 carries charge/discharge power limits.
BMS_04 carries contactor status and capacity. These are the frames Battery-Emulator
decodes to populate its datalayer.

NMH_Hybrid_01 is a classic CAN 2.0B frame (FD=false) on the FD-capable chip —
this exercises the mixed-mode capability from SPEC-008.

## Configurable parameters

At the top of `main.cpp`, `#define` constants let the user set the simulated battery
state without touching frame-packing logic:

```cpp
#define SIM_SOC_PERCENT      65     // 0–100
#define SIM_VOLTAGE_V        370    // pack voltage in volts
#define SIM_MAX_CHARGE_KW    22     // max charge power in kW
#define SIM_MAX_DISCHARGE_KW 100    // max discharge power in kW
```

These are encoded into the relevant frame bytes using the same scaling factors as
MEB-BATTERY.cpp. The user can change them and reflash to simulate different battery
states.

## Acceptance criteria

### AC-1 — Bus comes up
- `configure(500000, 2000000, MODE_NORMAL)` returns `CanStatus::OK`
- Serial prints `[MEB-SIM] CAN FD ready` on success
- Serial prints `[MEB-SIM] configure failed` and halts on failure

### AC-2 — BMS_20 transmitted correctly
- BMS_20 (0xCF) transmitted every 10 ms with BMS_mode = 1 in bits [2:0] of byte 2
- Rolling counter in bits [3:0] of byte 1 increments 0→1→2→...→15→0
- VAG CRC in byte 0 is correct for every counter value
  (verified by a second node reading the frame and checking the CRC)
- Frame is CAN FD (fdf=true, brs=true), 11-bit SID, DLC=8

### AC-3 — Full frame schedule running
- All frames in the schedule table above are transmitted at their correct intervals
- NMH_Hybrid_01 is transmitted as a classic CAN 2.0B frame (fdf=false)
- ESC_51_Auth is transmitted as a 48-byte FD frame (DLC=14)
- EM1_01 is transmitted as a 32-byte FD frame (DLC=10)
- No TX failures under normal bus conditions

### AC-4 — BMS_20 received and decoded from Battery-Emulator node
- When a Battery-Emulator node is connected and transmitting BMS_20 back,
  the simulator receives it and prints the BMS_mode to Serial:
  `[MEB-SIM] RX BMS_20 mode=1` (or whatever mode the remote node reports)
- This confirms two-way communication

### AC-5 — TX failures reported
- If `transmit()` returns anything other than `CanTxResult::OK`, Serial prints:
  `[MEB-SIM] TX FAIL <frame_name>`
- Bus errors reported via `hasErrors()` print:
  `[MEB-SIM] BUS ERROR`

### AC-6 — Build
- Example builds cleanly with `lib_deps = foodyfood/esp32-mcp2518fd-driver`
- No regressions in existing examples or integration harnesses

## platformio.ini

```ini
[env:meb_battery_sim]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = foodyfood/esp32-mcp2518fd-driver
upload_port = COM4
monitor_speed = 115200
```

## Source reference

All frame IDs, payloads, CRC constants, PDU tables and timing intervals are taken
directly from:
- `Battery-Emulator/Software/src/battery/MEB-BATTERY.h`
- `Battery-Emulator/Software/src/battery/MEB-BATTERY.cpp`

The VAG CRC function (`vw_crc_calc`) is ported directly from MEB-BATTERY.cpp.
The PDU constant tables (e.g. `BMS_20_PDU_CONST[16]`) are copied verbatim.
Every byte must be traceable to the Battery-Emulator source.

## File layout

```
examples/meb_battery_sim/
  platformio.ini
  src/
    main.cpp          — setup, loop, frame schedule, Serial output
    meb_frames.h      — frame definitions, PDU tables, VAG CRC
  README.md
```

Splitting frame definitions into `meb_frames.h` keeps `main.cpp` readable and makes
the frame content easy to audit against the Battery-Emulator source.

## Notes

- This example does not implement ISO-TP or UDS. It only emits the periodic keepalive
  frames that cause a real device to see a healthy battery. ISO-TP is a future extension.
- The VAG CRC output must be verified against known-good values from the
  Battery-Emulator source before the example is published.
- 29-bit IDs (BMS_22, BMS_21, KN_Hybrid_01, NMH_Hybrid_01) require `ext=true` in
  `CanMsg`. This is already supported by SPEC-001.
- The NMH_Hybrid_01 classic CAN frame requires `fdf=false, brs=false`. This is
  supported by SPEC-008.
- The ESC_51_Auth 48-byte frame uses DLC code 14 (`dlc=14`). This is within the
  driver's supported range (DLC 0–15).
- Added to `examples/` only after the package is published with all dependency specs
  Done. Do not add during spec work.

## Datasheet findings
_To be filled in during Step 1 of implementation._
