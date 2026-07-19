# Use Case Coverage

Real-world scenarios for the MCP2518FD driver, assessed against the current API.
**Scope: CAN FD only.** All use cases assume CAN FD frames (fdf=true, brs=true) throughout.
Battery-Emulator evidence is drawn from the cloned source at `../Battery-Emulator/`.

Coverage key: ✅ Covered — ⚠️ Partial — ❌ Gap

---

## UC-1 — EV Battery Pack Monitor / Gateway (BMS Gateway)

**Description**  
An ESP32 acts as a gateway between a CAN FD capable BMS and a vehicle or charger controller.
It listens to the BMS CAN FD bus, decodes cell voltages, temperatures and state-of-charge, and
re-publishes them on a second bus or over WiFi/MQTT. This is the core use case of
[Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator).

**Evidence from Battery-Emulator**  
- `comm_can.cpp` uses `ACAN2517FD` (wrapping MCP2518FD) for `CANFD_ADDON_MCP2518` interface  
- `KIA-64FD-BATTERY.h` sends and receives CAN FD frames (`FD=true`) at SIDs 0x10A, 0x120,
  0x7E4, 0x7EC — 11-bit, DLC up to 32 bytes, contactor-closing sequence of 63 pre-built frames  
- `MEB-BATTERY.cpp` (VW MEB) uses ISO-TP over CAN FD with 29-bit extended IDs
  (`ext_ID=true`, e.g. `0x1C42017B`) for UDS diagnostics; BMS_20 arrives every 10 ms  
- `comm_can.cpp` polls `canfd->hasCanErrors()` after every receive batch to detect bus errors  
- `CAN_frame` struct carries `bool FD`, `bool ext_ID`, `uint32_t ID`, `uint8_t DLC`,
  `uint8_t data[64]` — the full set of fields needed  
- `comm_can.cpp` calls `canfd->isr()` via GPIO interrupt — Battery-Emulator is interrupt-driven

**Typical bus parameters**  
- Nominal: 500 kbps  
- Data: 2 Mbps CAN FD (Kia 64 FD, VW MEB)  
- Frame IDs: fixed OEM SIDs, mix of 11-bit and 29-bit  
- Direction: mostly RX (monitor), periodic TX (keepalive / charge-enable frames every 10–100 ms)

| Feature required | Status | Evidence |
|---|---|---|
| Configure 500 kbps nominal / 2 Mbps data | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| CAN FD frames (fdf=true, brs=true) | ✅ | Verified in loopback and two_node |
| Non-blocking receive poll | ✅ | `available()` + `receive(msg)` |
| Blocking receive with timeout | ✅ | `receive(msg, timeoutMs)` |
| 29-bit extended ID (EID) | ❌ | `CanMsg.sid` is 11-bit only; MEB uses `ext_ID=true` with 29-bit IDs for ISO-TP |
| Filter to specific SIDs | ❌ | Only one catch-all filter; Battery-Emulator dispatches by `rx_frame.ID` in software — works but wastes CPU on high-traffic buses |
| Mixed 11-bit + 29-bit on same bus | ❌ | No EID support; cannot receive MEB-style 29-bit frames |
| Bus error / bus-off detection | ❌ | `comm_can.cpp` checks `canfd->hasCanErrors()` after every batch; no equivalent in this driver |
| RX overflow detection | ❌ | RXOVIF not surfaced; Battery-Emulator tracks `can_2518_bus_error` flag |
| Interrupt-driven RX | ❌ | Battery-Emulator uses `canfd->isr()` via GPIO interrupt; this driver is polling-only |
| stop() / restart() | ❌ | `comm_can.cpp` has `stop_can()` / `restart_can()`; no equivalent here |
| Runtime nominal rate change | ❌ | `setDataRate()` changes data phase only; no API to change nominal rate without full reconfigure |

---

## UC-2 — CAN FD Data Logger

**Description**  
A single-board logger captures every CAN FD frame on a bus (e.g. automotive ECU bench),
timestamps each frame, and streams them over USB-Serial or stores them to SD card.
Battery-Emulator has exactly this feature: `comm_can.cpp` logs every frame with a millisecond
timestamp to a ring buffer and streams it over USB Serial in candump format.

**Evidence from Battery-Emulator**  
- `print_can_frame()` in `comm_can.cpp` formats `(%lu.%02lu) RX0 <ID> [DLC] <bytes>` —
  standard candump format, streamed over Serial  
- `dump_can_frame()` writes to an in-memory ring buffer for the web UI  
- `user_selected_CAN_ID_cutoff_filter` — a runtime-configurable ID cutoff for the logger  
- Logs up to 64 bytes of CAN FD payload per frame  
- Processes up to 16 frames per poll cycle (`count++ < 16` loop in `_receive_frame_canfd()`)

**Typical bus parameters**  
- Nominal: 500 kbps – 1 Mbps  
- Data: 2–5 Mbps CAN FD  
- Direction: RX only (passive logger)

| Feature required | Status | Evidence |
|---|---|---|
| Configure nominal + data rate | ✅ | `configure(nominalBps, dataBps, mode)` |
| Receive CAN FD frames, all IDs | ✅ | Catch-all filter, `fdf` flag populated on receive |
| Receive 29-bit extended IDs | ❌ | No EID support; Battery-Emulator logs `frame.ID` as `uint32_t` (full 29-bit) |
| Per-frame RX timestamp | ❌ | RXTSEN / CiTBC not exposed; Battery-Emulator uses `millis()` as a workaround |
| Listen-only mode (passive, no ACK) | ⚠️ | `MODE_LISTEN` constant exists; not validated on real bus |
| Deep RX FIFO (>5 frames) | ❌ | FSIZE hardcoded to 4; a burst of 5+ frames before the poll loop runs will overflow |
| RX overflow flag | ❌ | RXOVIF not surfaced |
| Runtime ID cutoff filter | ❌ | No filter API; Battery-Emulator implements this in software |

---

## UC-3 — UDS Diagnostics over CAN FD (ISO-TP)

**Description**  
An ESP32-based tool sends UDS (ISO 14229) request frames to a vehicle ECU over CAN FD and
receives multi-frame ISO-TP responses. Battery-Emulator implements this fully for the VW MEB
battery (`MEB-BATTERY.cpp`): it sends UDS ReadDataByIdentifier (0x22) and ReadDTCInformation
(0x19) requests every 200 ms and reassembles multi-frame ISO-TP responses to read cell
voltages, SOC, SOH and DTCs.

**Evidence from Battery-Emulator**  
- `MEB-BATTERY.cpp` calls `isotp_send()` / `isotp_receive()` for UDS over CAN FD  
- ISO-TP TX frames use `ext_ID=true`, `ID=0x1C420017` (29-bit functional address)  
- ISO-TP RX frames arrive on `ID=0x1C42017B` (29-bit physical response address)  
- `KIA-64FD-BATTERY.cpp` sends UDS poll `0x7E4` (11-bit CAN FD) and receives on `0x7EC`  
- Flow-control frames must be sent within the ISO-TP BS timeout — requires low-latency TX

**Typical bus parameters**  
- Nominal: 500 kbps  
- Data: 2 Mbps CAN FD  
- Direction: bidirectional request/response

| Feature required | Status | Evidence |
|---|---|---|
| Configure 500 kbps / 2 Mbps | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| Transmit a CAN FD request frame | ✅ | `transmit(msg)` |
| Receive CAN FD response with timeout | ✅ | `receive(msg, 200)` |
| 29-bit extended IDs (ISO-TP functional/physical addresses) | ❌ | MEB uses `ext_ID=true`; not supported in CanMsg |
| Filter to specific SID (0x7EC response only) | ❌ | No filter API; all frames arrive in FIFO2 |
| TX error distinction (no ECU vs bus error) | ⚠️ | `transmit()` returns bool; no TXABT vs TXERR distinction exposed |
| Low-latency TX for ISO-TP flow control | ⚠️ | Polling-only; flow-control frame may be delayed if main loop is busy |

---

## UC-4 — EV Inverter / Motor Controller Interface

**Description**  
An ESP32 sends torque/speed setpoints to a CAN FD motor controller (e.g. Cascadia CM200DZ,
Emotor, Rinehart) and reads back telemetry (current, RPM, temperature) at high rate.
Battery-Emulator does this on the inverter side — `comm_can.cpp` transmits inverter frames
every 10–100 ms and checks for bus errors after each receive batch.

**Typical bus parameters**  
- Nominal: 500 kbps – 1 Mbps  
- Data: 2–5 Mbps CAN FD  
- Direction: bidirectional, high rate (setpoints every 10 ms)

| Feature required | Status | Evidence |
|---|---|---|
| Configure 500 kbps – 1 Mbps nominal / 2–5 Mbps data | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| Transmit CAN FD frame | ✅ | `transmit(msg)` |
| Non-blocking receive | ✅ | `available()` + `receive(msg)` |
| Runtime data rate switch | ✅ | `setDataRate(dataBps)` |
| TX error feedback (no ACK vs bus error) | ⚠️ | `transmit()` returns bool only |
| Bus error counters (TEC/REC) | ❌ | CiTREC not exposed |
| Interrupt-driven RX for low latency | ❌ | Polling only |

---

## UC-5 — Two-Node Peer-to-Peer CAN FD Link

**Description**  
Two ESP32 boards communicate directly over a short CAN FD cable — e.g. a sensor node sending
readings to a data concentrator, or a drone flight controller talking to an ESC board.
Covered by the existing `two_node` and `walkie_talkie` examples.

**Typical bus parameters**  
- Nominal: 125–500 kbps  
- Data: 2–5 Mbps CAN FD  
- Direction: bidirectional

| Feature required | Status | Evidence |
|---|---|---|
| Configure nominal + data rate | ✅ | `configure(125000, 2000000, MODE_NORMAL)` |
| Transmit CAN FD frame | ✅ | `transmit(msg)` |
| Receive with timeout | ✅ | `receive(msg, timeoutMs)` |
| Runtime data rate switch | ✅ | `setDataRate(dataBps)` |
| 64-byte payload (DLC=15) | ✅ | Verified in loopback and two_node |
| No-ACK retry at application level | ✅ | `txWithRetry()` pattern in two_node example |
| Detect other node absent | ⚠️ | `transmit()` returns false after 3 chip retries; no explicit bus-off event |

---

## UC-6 — Scope / Analyser Stimulus

**Description**  
A single board drives known CAN FD frames onto the bus for oscilloscope or protocol analyser
capture. No second node required — the chip ACKs its own frames in `MODE_EXTERNAL_LB`.
Covered by the existing `scope_loopback` example.

**Typical bus parameters**  
- Any rate  
- Direction: TX only

| Feature required | Status | Evidence |
|---|---|---|
| External loopback mode | ✅ | `MODE_EXTERNAL_LB` — real bus signals, self-ACK |
| Continuous TX | ✅ | Loop calling `transmit()` |
| Runtime rate switch | ✅ | `setDataRate()` |
| FSYS readback | ✅ | `getFsys()` |

---

## UC-7 — Production Self-Test / Bring-Up

**Description**  
Factory or field test that verifies the MCP2518FD chip and transceiver are wired correctly
before shipping. Covered by the existing `loopback` example.

| Feature required | Status | Evidence |
|---|---|---|
| Internal loopback (no bus) | ✅ | `MODE_INTERNAL_LB` |
| External loopback (transceiver check) | ✅ | `MODE_EXTERNAL_LB` |
| OSC frequency readback | ✅ | `getFsys()` / `readOsc()` |
| RATE_NOT_ACHIEVABLE detection | ✅ | `CanStatus::RATE_NOT_ACHIEVABLE` |
| Error status readback | ❌ | CiTREC / CiBDIAG0 not exposed |

---

## UC-8 — CAN FD Battery Simulator

**Description**  
An ESP32 impersonates a real EV battery pack on a CAN FD bus, transmitting the periodic BMS
frames that a vehicle, inverter or charger expects to see. The receiving device cannot tell the
difference between the simulator and a real battery. This is useful for bench-testing inverters,
chargers and BMS gateways without a real (expensive, heavy, dangerous) battery pack present.

Two batteries are targeted, drawn directly from the
[Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) source:

- **Kia 64 kWh FD** — 11-bit SIDs, CAN FD frames up to 32 bytes, a timed startup sequence of
  63 pre-built frames followed by periodic PID polling on SID 0x7E4 / response on 0x7EC.
  No CRC. Simpler — no 29-bit IDs required.

- **VW MEB** — 29-bit extended IDs throughout (e.g. `BMS_20=0xCF`, `BMS_21=0x12DD54D0`).
  VAG 0x2F-polynomial CRC on several frames. Multiple TX intervals: 10 / 20 / 40 / 50 / 100 /
  200 / 500 ms / 1 s. Requires the receiving device to see `HVK_01`, `ESC_51_Auth`,
  `Airbag_01`, `EM1_01` etc. before it will close contactors.

**Evidence from Battery-Emulator**  
- `KIA-64FD-BATTERY.cpp` — 63-frame startup sequence, then 200 ms PID poll loop  
- `MEB-BATTERY.cpp` — full multi-interval TX schedule, VAG CRC, 29-bit IDs, ISO-TP UDS  
- Both use `CAN_frame` with `bool FD`, `bool ext_ID`, `uint32_t ID`, `uint8_t DLC`, `uint8_t data[64]`

**Typical bus parameters**  
- Nominal: 500 kbps  
- Data: 2 Mbps CAN FD  
- Kia: 11-bit SIDs only  
- MEB: mix of 11-bit and 29-bit extended IDs

| Feature required | Status | Evidence |
|---|---|---|
| Configure 500 kbps / 2 Mbps, MODE_NORMAL | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| Transmit CAN FD frames up to 32 bytes | ✅ | `transmit(msg)`, DLC up to 15 |
| Timed TX schedule (10–1000 ms intervals) | ✅ | `millis()` interval pattern, no driver change needed |
| 11-bit SID TX and RX (Kia) | ✅ | Current `CanMsg.sid` field |
| 29-bit extended ID TX and RX (MEB) | ❌ | `CanMsg` has no EID field — blocks MEB entirely |
| Acceptance filter for response SIDs | ❌ | No filter API — all frames arrive in FIFO2 |
| TX error feedback | ⚠️ | `transmit()` returns bool only; no distinction between no-ACK and bus error |

---

## Gap Summary

Consolidated list of every ❌ gap across all use cases, ordered by impact.

| # | Gap | Blocks | Priority |
|---|---|---|---|
| G1 | **29-bit extended ID (EID)** — `CanMsg` has no EID field; TX object word 0 only encodes 11-bit SID; RX decode ignores EID bits | UC-1 (MEB), UC-3 (ISO-TP) | High |
| G2 | **Acceptance filters** — only one catch-all filter (mask=0); no per-SID, per-range or per-mask filter API | UC-1, UC-2, UC-3 | High |
| G3 | **Bus error / bus-off detection** — CiTREC, CiBDIAG0/1 not exposed; Battery-Emulator checks `hasCanErrors()` after every receive batch | UC-1, UC-4, UC-7 | High |
| G4 | **Interrupt-driven RX** — Battery-Emulator uses `canfd->isr()` via GPIO interrupt; polling-only misses frames on high-traffic buses | UC-1, UC-2, UC-4 | Medium |
| G5 | **RX overflow detection** — RXOVIF / FIFOSTA_RXOVIF not surfaced to caller | UC-1, UC-2 | Medium |
| G6 | **Per-frame RX timestamp** — RXTSEN / CiTBC not exposed; Battery-Emulator falls back to `millis()` | UC-1, UC-2 | Medium |
| G7 | **stop() / restart()** — no way to pause and resume the chip; Battery-Emulator calls `canfd->end()` / `begin()` for BMS reset sequences | UC-1 | Medium |
| G8 | **Configurable RX FIFO depth** — FSIZE hardcoded to 4 in `configFifos()`; a burst of 5+ back-to-back frames overflows before the poll loop runs | UC-2 | Low |
| G9 | **TX error distinction** — `transmit()` returns bool; no way to tell no-ACK from bus-error from FIFO-full | UC-3, UC-4 | Low |
| G10 | **Listen-only mode validation** — `MODE_LISTEN` constant exists but untested on real bus | UC-1, UC-2 | Low |

---

## API Clarity Notes

Observations from reading the API as a new user, informed by how Battery-Emulator structures
its own CAN FD abstraction (`CAN_frame`, `CanReceiver`, `comm_can.h`).

- **No EID field in CanMsg.** Battery-Emulator's `CAN_frame` has `bool ext_ID` and `uint32_t ID`
  (full 29-bit). The absence of this in `CanMsg` is the single biggest blocker for real-world
  EV battery work — VW MEB, Rivian, Ford Mach-E all use 29-bit IDs on their CAN FD buses.

- **`brs=true` without `fdf=true` is silently accepted** on transmit and will produce a
  malformed frame. The two flags should be coupled or at least validated — if `fdf=false` then
  `brs` is meaningless and should be ignored or asserted false.

- **No `lenToDlc()` inverse of `dlcToLen()`.** Users building frames from a byte buffer (e.g.
  assembling an ISO-TP payload) have to implement this themselves.

- **`transmit()` returns bool with no detail.** Battery-Emulator tracks separate
  `can_2518_send_fail` flags and calls `hasCanErrors()` to distinguish bus errors from send
  failures. A `CanTxResult` enum (OK / NO_ACK / BUS_ERROR / FIFO_FULL / TIMEOUT) would let
  callers react appropriately rather than just logging "TX failed".

- **`getMode()` returns `uint8_t`.** Returning a named enum or at least documenting the
  `MODE_*` constants in the return-value doc comment would reduce look-up friction.

- **No `deinit()` / `sleep()`.** Battery-Emulator calls `canfd->end()` to stop the chip during
  BMS reset sequences. `MODE_SLEEP` exists as a constant but is not documented or tested.

- **`configure()` is the only init path that detects FSYS.** `configureRaw()` skips detection.
  If a user calls `configureRaw()` first and then `setDataRate()`, `mFsys` is 0 and the
  calculation silently falls back to 20 MHz. This should be documented or guarded.

- **Single RX FIFO for all frames.** Battery-Emulator dispatches by `rx_frame.ID` in a
  `switch` statement after receiving — this works but means every frame wakes the CPU regardless
  of relevance. On a busy CAN FD bus (VW MEB sends frames every 10 ms on ~15 IDs) this is
  manageable in polling mode but would benefit from hardware filtering to reduce CPU load.
