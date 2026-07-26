# Use Case Coverage

Real-world scenarios for the MCP2518FD driver, assessed against the current API.
Covers CAN FD and Classic CAN 2.0B. Battery-Emulator evidence is drawn from the cloned source at `../Battery-Emulator/`.

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
| 29-bit extended ID (EID) | ✅ | `CanMsg.ext=true`, `CanMsg.id` carries full 29-bit EID |
| Filter to specific SIDs | ✅ | `setFilter(index, id, mask, ext)` — up to 32 filter slots |
| Mixed 11-bit + 29-bit on same bus | ✅ | Verified in id_filter harness |
| Bus error / bus-off detection | ✅ | `readAndClearErrors()`, `hasErrors()`, `CanError` struct |
| RX overflow detection | ✅ | `readAndClearErrors().rxOverflow`, cleared on read |
| sleep() / wake() | ✅ | `sleep()` disables oscillator; `wake()` restores previous mode |
| Interrupt-driven RX | ✅ | `MCP2518Driver(spi, cs, intPin)` — ISR sets flag, `available()` returns immediately |
| stop() / restart() | ✅ | `stop()` enters config mode, `restart()` restores previous mode |
| Runtime nominal rate change | ❌ | `setDataRate()` changes data phase only; full reconfigure needed for nominal rate |

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
| Receive 29-bit extended IDs | ✅ | `CanMsg.ext=true`, `CanMsg.id` carries full 29-bit EID |
| Per-frame RX timestamp | ✅ | `configure(..., enableTimestamp=true)`, `msg.timestamp` (TBC counts, 50 ns resolution at 20 MHz) |
| Listen-only mode (passive, no ACK) | ✅ | `MODE_LISTEN` validated on real bus; `transmit()` returns `NoAck` immediately |
| Deep RX FIFO (>5 frames) | ✅ | `configure(..., rxFifoDepth=N)`, up to 24 slots |
| RX overflow flag | ✅ | `getErrors().rxOverflow`, cleared on read |
| Runtime ID cutoff filter | ❌ | No range-cutoff filter API; implement in software |

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
| 29-bit extended IDs (ISO-TP functional/physical addresses) | ✅ | `CanMsg.ext=true`, `CanMsg.id` carries full 29-bit EID |
| Filter to specific SID (0x7EC response only) | ✅ | `setFilter(index, id, mask, ext)` |
| TX error distinction (no ECU vs bus error) | ✅ | `CanTxResult` enum: OK / NoAck / BusError / FifoFull |
| Low-latency TX for ISO-TP flow control | ✅ | INT pin ISR (`MCP2518Driver(spi, cs, intPin)`) wakes on incoming frame — flow-control TX can be issued immediately from the receive handler |

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
| TX error feedback (no ACK vs bus error) | ✅ | `CanTxResult` enum: OK / NoAck / BusError / FifoFull |
| Bus error counters (TEC/REC) | ✅ | `readAndClearErrors()` returns `CanError` with tec, rec, busOff |
| Interrupt-driven RX for low latency | ✅ | `MCP2518Driver(spi, cs, intPin)` — ISR wakes on frame arrival |

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
| Detect other node absent | ✅ | `transmit()` returns `CanTxResult::NoAck` after 3 chip retries — explicit, not just a bool |

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
| Error status readback | ✅ | `readAndClearErrors()` returns `CanError` with tec, rec, busOff, rxOverflow |

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
| 29-bit extended ID TX and RX (MEB) | ✅ | `CanMsg.ext=true`, `CanMsg.id` carries full 29-bit EID |
| Acceptance filter for response SIDs | ✅ | `setFilter(index, id, mask, ext)` — up to 32 slots |
| TX error feedback | ✅ | `CanTxResult` enum: OK / NoAck / BusError / FifoFull |

---

## UC-9 — Classic CAN 2.0B Gateway

**Description**  
An ESP32 bridges a legacy CAN 2.0B bus (classic-only nodes, no FD capability) to a CAN FD
network, or acts as a node on a mixed bus where some devices are classic-only. The driver
must transmit and receive classic frames without triggering error frames on classic nodes.

**Typical bus parameters**  
- Nominal: 125 kbps – 1 Mbps (no data phase)  
- Direction: bidirectional

| Feature required | Status | Evidence |
|---|---|---|
| Classic CAN mode | ✅ | `configure(nominalBps, 0, MODE_CLASSIC)` — REQOP=110 |
| Classic frame TX (fdf=false) | ✅ | `transmit(msg)` with `msg.fdf=false` |
| Classic frame RX | ✅ | `receive(msg)` — `msg.fdf=false` on received classic frames |
| FD frame rejected in classic mode | ✅ | `transmit()` returns `CanTxResult::InvalidMode` if `msg.fdf=true` |
| `setDataRate()` rejected in classic mode | ✅ | Returns `CanStatus::INVALID_MODE` |
| Two-node classic exchange | ✅ | Verified on real bus at 500 kbps |

---

## UC-10 — Dual-Chip Board (LilyGo T-2CAN FD)

**Description**  
Boards like the LilyGo T-2CAN FD carry two MCP2518FD chips driven by one ESP32. The second
chip has no crystal of its own — it is clocked from the first chip's CLKO output pin. The
first chip must be configured to output a clock on CLKO before the second chip can initialise.

**Typical configuration**  
- First chip: 40 MHz crystal, `clkoDivider=10` → CLKO = 4 MHz  
- Second chip: no crystal, `PLLEN=1` → FSYS = 40 MHz from 4 MHz PLL input

| Feature required | Status | Evidence |
|---|---|---|
| CLKO output pin configuration | ✅ | `CanConfig.clkoDivider` — 1/2/4/10 or 0 (leave default) |
| CLKODIV encoding (OSC bits 6:5) | ✅ | `clkoDivToReg()` — 00=÷1, 01=÷2, 10=÷4, 11=÷10 |
| Invalid divider rejected | ✅ | Returns `CanStatus::RATE_NOT_ACHIEVABLE` |
| INT1 pin for second chip RX interrupt | ✅ | `MCP2518Driver(spi, cs, NO_INT_PIN, NO_INT_PIN, int1Pin)` — INT1 used when INT is not wired (SPEC-010) |
| Second chip clocked from first | ⚠️ | Register readback confirms CLKODIV set; T-2CAN FD hardware not available for end-to-end test |

---

## UC-11 — ISOBUS / Precision Agriculture Monitor

**Description**  
ISO 11783 (ISOBUS) is the agricultural machinery standard built on classic CAN at 250 kbps.
An ESP32 in listen-only mode can silently tap a tractor's ISOBUS backbone and decode PGNs:
engine load, fuel consumption rate, GPS position, implement status, and section control.
No risk of disrupting the machine — the node never transmits. Useful for precision agriculture
data collection, fleet telematics, and yield mapping without modifying the tractor's ECU.

**Typical bus parameters**  
- Nominal: 250 kbps (ISOBUS mandated)  
- No data phase (classic CAN only)  
- Direction: RX only (passive tap)

| Feature required | Status | Evidence |
|---|---|---|
| Classic CAN mode at 250 kbps | ✅ | `configure(250000, 0, MODE_CLASSIC)` |
| Listen-only mode (no ACK, no disruption) | ✅ | `MODE_LISTEN` — validated on real bus |
| Receive classic frames, all IDs | ✅ | Catch-all filter, `msg.fdf=false` on receive |
| 29-bit extended IDs (ISOBUS PGNs use EID) | ✅ | `CanMsg.ext=true`, `CanMsg.id` carries full 29-bit EID |
| Per-frame RX timestamp | ✅ | `msg.timestamp` at 50 ns resolution |
| Deep RX FIFO for burst traffic | ✅ | `configure(..., rxFifoDepth=N)` |
| RX overflow detection | ✅ | `getErrors().rxOverflow` |

---

## UC-12 — Low-Power Sleep/Wake Sensor Node

**Description**  
A battery-powered sensor node (e.g. tyre pressure, temperature, or door status in a vehicle)
spends most of its time in deep sleep. On a scheduled interval or when a specific CAN frame
arrives on the INT pin, it wakes, reads its sensor, transmits one frame, and returns to sleep.
Directly enabled by `sleep()` / `wake()` (SPEC-006) and interrupt-driven RX (SPEC-004).

**Typical bus parameters**  
- Nominal: 125–500 kbps  
- Data: 2 Mbps CAN FD  
- Direction: TX-dominant, brief RX on wake trigger

| Feature required | Status | Evidence |
|---|---|---|
| sleep() / wake() | ✅ | `sleep()` puts chip in low-power mode; `wake()` restores |
| Interrupt-driven wake on frame arrival | ✅ | `MCP2518Driver(spi, cs, intPin)` — INT pin wakes ISR |
| Filter to wake-trigger frame only | ✅ | `setFilter(index, id, mask, ext)` — ignore all other traffic |
| Transmit one frame and return to sleep | ✅ | `transmit(msg)` then `sleep()` |
| TX error feedback | ✅ | `CanTxResult` — detect bus absent before sleeping again |

---

## UC-13 — OBD-FD Live Data Reader

**Description**  
Post-2023 vehicle platforms (e.g. GM VCSII, Ford, Stellantis) have migrated powertrain buses
to CAN FD. Classic OBD-II PIDs (mode 0x01) are still supported but now carried over CAN FD
frames. An ESP32 can send OBD-II requests on 0x7DF (functional) and receive responses on
0x7E8–0x7EF, enabling a DIY dashboard, track data logger, or emissions monitor without a
commercial scan tool.

**Typical bus parameters**  
- Nominal: 500 kbps  
- Data: 2 Mbps CAN FD  
- Direction: bidirectional request/response

| Feature required | Status | Evidence |
|---|---|---|
| Configure 500 kbps / 2 Mbps | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| Transmit 11-bit SID request (0x7DF) | ✅ | `transmit(msg)` with `msg.fdf=true` |
| Receive response with timeout | ✅ | `receive(msg, timeoutMs)` |
| Filter to response SID range (0x7E8–0x7EF) | ✅ | `setFilter(index, 0x7E8, 0x7F8, false)` — mask covers 8 IDs |
| TX error distinction (no ECU present) | ✅ | `CanTxResult::NoAck` |

---

## UC-14 — Robotics CAN FD Actuator Bus

**Description**  
Modern robot joints and servo drives (e.g. Moteus, ODrive, Unitree) communicate over CAN FD
at 1 Mbps nominal / 5 Mbps data. An ESP32 acts as a USB↔CAN FD bridge: a ROS 2 node on a
Linux host sends setpoints over USB Serial; the ESP32 forwards them as CAN FD frames to
actuators and streams telemetry back. Useful for mobile robots, cobots, and drone ESC buses
where a full Linux SBC is too heavy or power-hungry for the CAN interface role.

**Typical bus parameters**  
- Nominal: 1 Mbps  
- Data: 5 Mbps CAN FD  
- Direction: bidirectional, 1 ms control loop

| Feature required | Status | Evidence |
|---|---|---|
| Configure 1 Mbps nominal / 5 Mbps data | ✅ | `configure(1000000, 5000000, MODE_NORMAL)` |
| Transmit CAN FD frame | ✅ | `transmit(msg)` |
| Interrupt-driven RX for 1 ms latency | ✅ | `MCP2518Driver(spi, cs, intPin)` |
| Per-frame timestamp for control loop timing | ✅ | `msg.timestamp` at 50 ns resolution |
| Runtime data rate switch (actuator negotiation) | ✅ | `setDataRate(dataBps)` |
| Bus error detection | ✅ | `getErrors()` — detect actuator fault or wiring issue |

---

## UC-15 — Marine NMEA 2000 Monitor

**Description**  
NMEA 2000 is the marine electronics network standard — classic CAN at 250 kbps with a
specific PGN framing. An ESP32 in listen-only mode can decode GPS position, depth, wind speed,
AIS targets, and engine data from a boat's N2K backbone and re-publish over WiFi to a phone
app or chart plotter. The node is completely invisible to other N2K devices — it never
transmits and cannot cause a bus fault.

**Typical bus parameters**  
- Nominal: 250 kbps (N2K mandated)  
- No data phase (classic CAN only)  
- Direction: RX only

| Feature required | Status | Evidence |
|---|---|---|
| Classic CAN mode at 250 kbps | ✅ | `configure(250000, 0, MODE_CLASSIC)` |
| Listen-only mode | ✅ | `MODE_LISTEN` |
| Receive classic frames, all IDs | ✅ | Catch-all filter |
| 29-bit extended IDs (N2K PGNs use EID) | ✅ | `CanMsg.ext=true` |
| Deep RX FIFO for burst traffic | ✅ | `configure(..., rxFifoDepth=N)` |
| Per-frame timestamp | ✅ | `msg.timestamp` |

---

## UC-16 — DC Fast Charger Power Module Interface

**Description**  
DC fast chargers (CCS, CHAdeMO) use CAN FD internally between the charger controller and
power modules (e.g. Delta, Brusa, Elcon). An ESP32 can monitor or simulate the power module
side of this bus for bench-testing a charger controller without live high-voltage hardware.
Useful for EVSE manufacturers, EV conversion builders, and anyone developing a custom
charging solution.

**Typical bus parameters**  
- Nominal: 250–500 kbps  
- Data: 2 Mbps CAN FD  
- Direction: bidirectional (controller ↔ power module)

| Feature required | Status | Evidence |
|---|---|---|
| Configure 250–500 kbps / 2 Mbps | ✅ | `configure(500000, 2000000, MODE_NORMAL)` |
| Transmit CAN FD frames (power module simulation) | ✅ | `transmit(msg)` |
| Receive controller commands with timeout | ✅ | `receive(msg, timeoutMs)` |
| Listen-only mode (passive monitor) | ✅ | `MODE_LISTEN` |
| Filter to specific command IDs | ✅ | `setFilter(index, id, mask, ext)` |
| TX error feedback (controller absent) | ✅ | `CanTxResult::NoAck` |
| stop() / restart() for safe HV sequencing | ✅ | `stop()` / `restart()` |

---

## Gap Summary

Consolidated list of every gap across all use cases, ordered by impact.

| # | Gap | Blocks | Status |
|---|---|---|---|
| G1 | **29-bit extended ID (EID)** | UC-1 (MEB), UC-3 (ISO-TP) | ✅ Closed (SPEC-001) |
| G2 | **Acceptance filters** | UC-1, UC-2, UC-3 | ✅ Closed (SPEC-002) |
| G3 | **Bus error / bus-off detection** | UC-1, UC-4, UC-7 | ✅ Closed (SPEC-003) |
| G4 | **Interrupt-driven RX** | UC-1, UC-2, UC-4 | ✅ Closed (SPEC-004) |
| G5 | **RX overflow detection** | UC-1, UC-2 | ✅ Closed (SPEC-003) |
| G6 | **Per-frame RX timestamp** | UC-1, UC-2 | ✅ Closed (SPEC-005) |
| G7 | **stop() / restart()** | UC-1 | ✅ Closed (SPEC-006) |
| G8 | **Configurable RX FIFO depth** | UC-2 | ✅ Closed (SPEC-004) |
| G9 | **TX error distinction** | UC-3, UC-4 | ✅ Closed (SPEC-003) |
| G10 | **Listen-only mode validation** | UC-1, UC-2 | ✅ Closed (SPEC-005) |
| IR-18 | **Classic CAN mode** | UC-9 | ✅ Closed (SPEC-008) |
| IR-19 | **CLKO output configuration** | UC-10 | ✅ Closed (SPEC-009) |
| IR-20 | **Dual INT pin support (INT0/INT1)** | UC-10 | ✅ Closed (SPEC-010) |

---

## API Clarity Notes

Observations from reading the API as a new user, informed by how Battery-Emulator structures
its own CAN FD abstraction (`CAN_frame`, `CanReceiver`, `comm_can.h`).

- **`brs=true` without `fdf=true` is silently accepted** on transmit and will produce a
  malformed frame. The two flags should be coupled or at least validated.

- **No `lenToDlc()` inverse of `dlcToLen()`.** Users building frames from a byte buffer (e.g.
  assembling an ISO-TP payload) have to implement this themselves.

- **`getMode()` returns `uint8_t`.** The `MODE_*` constants are documented in `docs/api.md`.

- **`stop()` / `sleep()` / `wake()` / `restart()` are implemented** (SPEC-006). ✅

- **`readAndClearErrors()` was `getErrors()` before SPEC-007.** Any code referencing `getErrors()` needs updating.

- **`configure()` is the only init path that detects FSYS.** `configureRaw()` skips detection.
  If a user calls `configureRaw()` first and then `setDataRate()`, `mFsys` is 0 and the
  calculation silently falls back to 20 MHz. Documented in `docs/api.md` under `configureRaw()`.
