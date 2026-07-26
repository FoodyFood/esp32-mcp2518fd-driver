# UC-DALA: Battery-Emulator Integration

**Project:** https://github.com/dalathegreat/Battery-Emulator  
**Goal:** Replace the ACAN2517FD library with our MCP2518FD driver as the CAN FD
transport for the `CANFD_ADDON_MCP2518` and `CANFD_ADDON_MCP2518_2` interfaces.  
**Scope:** Integration requirements only. Gap analysis is a separate step.

---

## 1. What Battery-Emulator is

A production firmware for ESP32 that bridges real EV battery packs (Kia 64 FD, VW MEB,
BMW iX, etc.) to solar inverters over CAN, CAN FD, RS485 and Modbus. It runs on several
hardware variants (LilyGo T-CAN485, LilyGo T-2CAN, Stark CMR, BECom, Waveshare). The
CAN FD path — the one we care about — is used by the Kia 64 FD and VW MEB battery
implementations, both of which are real-world production protocols.

---

## 2. Current CAN FD implementation

The CAN FD transport lives entirely in one file:
`Software/src/communication/can/comm_can.cpp`

It uses the **ACAN2517FD** library (vendored at
`Software/src/lib/pierremolinaro-ACAN2517FD/`). The library is instantiated as a
module-level pointer:

```cpp
static ACAN2517FD* canfd   = nullptr;   // first chip
static ACAN2517FD* canfd_2 = nullptr;   // second chip (dual-CAN FD boards)
```

The public surface used from that library is:

| Call | Where used |
|---|---|
| `new ACAN2517FD(cs, spi, int_pin, int0, int1)` | `init_CAN()` |
| `canfd->begin(*settings, isr_lambda)` | `begin_canfd()` |
| `canfd->poll()` | immediately after `begin()` |
| `canfd->isr()` | inside the ISR lambda passed to `begin()` |
| `canfd->available()` | `_receive_frame_canfd()` |
| `canfd->receive(CANFDMessage&)` | `_receive_frame_canfd()` |
| `canfd->tryToSend(CANFDMessage&)` | `transmit_can_frame_to_interface()` |
| `canfd->hasCanErrors()` | `_receive_frame_canfd()` |
| `canfd->end()` | `stop_can()` / `restart_can()` |

Configuration is done through `ACAN2517FDSettings`:

```cpp
settings2517 = new ACAN2517FDSettings(osc_freq, bitRate, DataBitRateFactor::x4);
settings2517->mCLKOPin       = ...;  // clock output divider for second chip
settings2517->mRequestedMode = use_canfd_as_can
    ? ACAN2517FDSettings::Normal20B   // classic CAN frames only
    : ACAN2517FDSettings::NormalFD;   // CAN FD frames
```

The oscillator frequency is either autodetected or read from the HAL:
`esp32hal->MCP2517_FREQ()` returns 0 (autodetect), 20000000, or 40000000.

---

## 3. CAN frame data model

Battery-Emulator's internal frame type is `CAN_frame`
(`Software/src/devboard/utils/types.h`):

```cpp
typedef struct {
  bool     FD;       // true = CAN FD frame
  bool     ext_ID;   // true = 29-bit extended ID
  uint8_t  DLC;      // byte count 0–64, NOT the 4-bit DLC code
  uint32_t ID;
  union {
    uint8_t  u8[64];
    uint32_t u32[2];
    uint64_t u64;
  } data;
} CAN_frame;
```

Key observations:

- `DLC` is the **byte count** (0–64), not the 4-bit DLC code. ACAN2517FD also uses
  byte count in `CANFDMessage.len`. Our driver uses the 4-bit DLC code internally but
  exposes byte count via `dlcToLen()` — the mapping must be applied at the integration
  boundary.
- `FD = false` means a classic CAN 2.0B frame sent over the FD-capable chip. The
  emulator uses this for some frames even on the FD interface (e.g. MEB's NMH_Klima,
  NMH_Gateway, NMH_DCDC_NV, Kombi_02).
- `ext_ID = true` means 29-bit EID. The MEB battery uses many 29-bit IDs
  (e.g. `0x18DA05F1`, `0x1C40007B`, `0x1A555550`). Kia 64 FD uses 11-bit only.
- Payload sizes seen in the wild: 4, 8, 32, 48 bytes. DLC 48 maps to DLC code 14.
  Our driver supports DLC 0–15 (up to 64 bytes).

---

## 4. Interface enumeration

Battery-Emulator defines five CAN interfaces:

```cpp
enum CAN_Interface {
  CAN_NATIVE            = 0,  // ESP32 TWAI — not our concern
  CANFD_NATIVE          = 1,  // Stark CMR onboard FD — same chip, same library
  CAN_ADDON_MCP2515     = 2,  // MCP2515 — not our concern
  CANFD_ADDON_MCP2518   = 3,  // first MCP2518FD — primary target
  CANFD_ADDON_MCP2518_2 = 4,  // second MCP2518FD — dual-CAN FD boards
  NO_CAN_INTERFACE      = 5
};
```

`CANFD_NATIVE` (Stark CMR) uses the same ACAN2517FD library and the same `canfd`
pointer — it is treated identically to `CANFD_ADDON_MCP2518` in the receive path.
Replacing the library replaces both.

---

## 5. Hardware variants that use CAN FD

| Board | Chip | Oscillator | SPI bus | CS | INT | Notes |
|---|---|---|---|---|---|---|
| LilyGo T-2CAN (FD) | MCP2518FD | 40 MHz | FSPI (S3) | GPIO10 | INT0=GPIO9, INT1=GPIO3 | Two INT pins, no single INT |
| LilyGo T-2CAN (FD) — 2nd chip | MCP2518FD | 0 (autodetect) | HSPI (S3) | GPIO41 | GPIO39 | Clocked from 1st chip CLKO |
| Stark CMR | MCP2518FD | 40 MHz | HSPI (ESP32) | GPIO18 | GPIO35 | Single INT pin |
| Stark CMR — 2nd chip (add-on) | MCP2518FD | — | shared with 1st | GPIO12 | GPIO14 | CS+INT only |
| BECom | MCP2518FD | unknown | — | — | — | HAL not inspected |
| Waveshare | MCP2518FD | unknown | — | — | — | HAL not inspected |

The T-2CAN uses **two INT pins** (INT0/INT1) rather than a single INT. The ACAN2517FD
constructor accepts all three: `ACAN2517FD(cs, spi, int_pin, int0_pin, int1_pin)`.
Our driver currently accepts a single INT pin. This is a gap to assess.

---

## 6. Bit rate configuration

Battery-Emulator configures the FD chip with:
- Nominal rate: whatever `CAN_Speed` the receiver registered (100–1000 kbps)
- Data rate: `DataBitRateFactor::x4` — 4× the nominal rate
  - 500 kbps nominal → 2 Mbps data (most common)
  - 250 kbps nominal → 1 Mbps data
- The `use_canfd_as_can` flag switches the chip to `Normal20B` mode (classic CAN only,
  no BRS). This is a runtime-configurable user setting.

Our driver's `configure(nominal, data, mode)` maps directly to this. The `MODE_NORMAL`
vs classic-only distinction maps to our `MODE_NORMAL` (FD) vs a mode that disables BRS.
Currently our driver has no "FD capable but BRS disabled" mode — this is a gap.

---

## 7. Transmit path requirements

From `transmit_can_frame_to_interface()`:

1. Check `allowed_to_send_CAN` (safety interlock) — Battery-Emulator's concern.
2. Map `CAN_frame` → `CANFDMessage`:
   - `FD = true`  → `CANFDMessage::CANFD_WITH_BIT_RATE_SWITCH`
   - `FD = false` → `CANFDMessage::CAN_DATA` (classic frame on FD chip)
3. Copy `ID`, `ext`, `len`, `data`.
4. Call `canfd->tryToSend(frame)` — non-blocking, returns bool.
5. On failure, set `datalayer.system.info.can_2518_send_fail = true`.

Requirements for our driver:
- Non-blocking transmit that returns success/failure immediately.
- Transmit classic CAN 2.0B frames (`FD=false`) on the FD-capable chip without BRS.
- Transmit CAN FD frames with BRS.
- Support 29-bit extended IDs.
- Support DLC up to 64 bytes (DLC code 15).
- Support DLC 48 bytes (DLC code 14) — seen in MEB's ESC_51_Auth frame.

---

## 8. Receive path requirements

From `_receive_frame_canfd()`:

1. Poll `canfd->available()` in a loop (up to 16 frames per call).
2. Call `canfd->receive(CANFDMessage&)` for each available frame.
3. Map back to `CAN_frame` and dispatch to registered receivers.
4. Check `canfd->hasCanErrors()` and set bus error flag.

The receive loop runs from the main Arduino loop — it is **polling-based**, not
interrupt-driven at the application level. The ISR is used internally by ACAN2517FD
to move frames from chip RAM into a software FIFO; the application polls that FIFO.

Requirements for our driver:
- `available()` — non-blocking check for received frames.
- `receive(frame)` — non-blocking dequeue of one frame.
- `hasCanErrors()` equivalent — bus error query.
- The ISR must be self-managed by the driver using the INT pin; no ISR callback
  required from the caller.

---

## 9. Lifecycle requirements

| Operation | ACAN2517FD call | Context |
|---|---|---|
| Init | `begin(*settings, isr_lambda)` + `poll()` | `init_CAN()` at startup |
| Stop | `end()` | `stop_can()` — before OTA, contactor open, etc. |
| Restart | `begin(*settings, isr_lambda)` again | `restart_can()` |
| Speed change | Not supported for FD interfaces | — |

Our driver's `configure()` / `stop()` / `restart()` map to these. The restart path
calls `begin_canfd()` which re-runs the full `begin()` — equivalent to calling our
`configure()` again.

---

## 10. Error reporting requirements

Battery-Emulator tracks three error flags per FD interface:
- `can_2518_send_fail` — set when `tryToSend()` returns false
- `can_2518_bus_error` — set when `hasCanErrors()` returns true
- `can_2518_2_send_fail` / `can_2518_2_bus_error` — same for second chip

These are displayed in the web UI and used by the safety module. Our driver needs:
- Non-blocking transmit that returns a failure status.
- A way to query whether a bus error has occurred (TEC/REC overflow, bus-off).

---

## 11. Dual-chip requirement

Two independent MCP2518FD chips can be present simultaneously (T-2CAN FD, Stark CMR
with add-on). They share the same SPI bus but have separate CS and INT pins.
Battery-Emulator instantiates two separate `ACAN2517FD` objects. Our driver would be
instantiated twice with different `SPIClass` + CS pin combinations — which it already
supports via the constructor.

The second chip on T-2CAN uses a different SPI bus (HSPI vs FSPI). Our driver takes
an `SPIClass&` reference, so two instances on different buses is already supported.

---

## 12. Clock output (CLKO) requirement

The T-2CAN uses the first MCP2518FD's CLKO pin to clock the second chip:

```cpp
settings2517->mCLKOPin = static_cast<ACAN2517FDSettings::CLKOpin>(esp32hal->MCP2517_CLKODIV());
```

`MCP2517_CLKODIV()` returns `0b11` by default (divide by 10 → 4 MHz from 40 MHz
oscillator). The second chip's oscillator frequency is set to 0 (autodetect) because
it receives its clock from the first chip's CLKO output.

Our driver must be able to configure the CLKO output pin and divider, or at minimum
not disable it. Our driver currently does not expose CLKO configuration — this is a gap.

---

## 13. Normal20B / classic-CAN-on-FD-chip mode

`use_canfd_as_can` is a user-configurable flag. When true, the chip is put into
`Normal20B` mode — it operates as a classic CAN 2.0B controller (no FD frames, no
BRS). This is used when the battery protocol is classic CAN but the hardware only has
an MCP2518FD chip available.

Our driver's `MODE_NORMAL` always enables FD. We have no mode that configures the chip
as a classic CAN controller. This is a gap.

---

## 14. Integration point — exactly where and how

The integration is a **source-level replacement** inside Battery-Emulator. There is no
PlatformIO `lib_deps` entry for ACAN2517FD — it is vendored directly in
`Software/src/lib/pierremolinaro-ACAN2517FD/`. The replacement options are:

### Option A — Drop-in source replacement (adapter layer)
Replace the vendored ACAN2517FD source files with a thin adapter that wraps our driver
behind the same `ACAN2517FD` / `ACAN2517FDSettings` / `CANFDMessage` API.
Battery-Emulator code is unchanged. Our driver is the implementation behind the facade.

- Integration point: `Software/src/lib/pierremolinaro-ACAN2517FD/`
- No changes to `comm_can.cpp` or any battery/inverter file.
- Requires writing an adapter that maps `ACAN2517FDSettings` → our `configure()` and
  `CANFDMessage` ↔ `CanMsg`.

### Option B — Direct replacement in comm_can.cpp (recommended)
Remove ACAN2517FD entirely. Rewrite the `canfd` / `canfd_2` section of `comm_can.cpp`
to use our `MCP2518Driver` directly. Add our library via `lib_deps` in `platformio.ini`.

- Integration point: `Software/src/communication/can/comm_can.cpp` + `platformio.ini`
- Cleaner — no adapter layer, direct use of our public API.
- Change confined to one file (~50 lines).
- Our library added as:
  ```ini
  lib_deps = file://../../path/to/mcp2518fd
  ; or: foodyfood/esp32-mcp2518fd-driver
  ```

### Option C — Upstream contribution
Submit a PR to Battery-Emulator that adds our driver as an alternative backend,
selectable via a build flag. Requires maintainer buy-in.

**Recommended path: Option B.** Cleanest integration, uses our public API as intended,
change confined to one file.

---

## 15. `#include` and build integration

Under Option B, `comm_can.cpp` changes from:

```cpp
#include "../../lib/pierremolinaro-ACAN2517FD/ACAN2517FD.h"
```

to:

```cpp
#include "mcp2518fd_can.h"
```

And `platformio.ini` adds to each relevant env:

```ini
lib_deps =
    foodyfood/esp32-mcp2518fd-driver
```

Our library has no external dependencies beyond Arduino SPI, which Battery-Emulator
already uses. No RTOS, no heap allocation beyond what already exists.

---

## 16. API mapping — ACAN2517FD → MCP2518Driver

| ACAN2517FD | MCP2518Driver equivalent | Notes |
|---|---|---|
| `new ACAN2517FD(cs, spi, int)` | `MCP2518Driver can(spi, cs)` | INT pin wired, driver self-manages ISR |
| `begin(settings, isr_lambda)` | `configure(nominal, data, mode)` | ISR self-managed |
| `poll()` after begin | not needed | driver handles internally |
| `tryToSend(CANFDMessage&)` | `transmit(CanMsg&)` returns `CanStatus` | map `CANFDMessage` → `CanMsg` |
| `available()` | `available()` | identical semantics |
| `receive(CANFDMessage&)` | `receive(CanMsg&)` | non-blocking, map back |
| `hasCanErrors()` | TEC/REC / bus-off query | needs assessment |
| `end()` | `stop()` | |
| re-`begin()` | `restart()` | |
| `CANFD_WITH_BIT_RATE_SWITCH` | `CanMsg.fdf=true, brs=true` | |
| `CAN_DATA` (classic on FD chip) | `CanMsg.fdf=false` | |
| `CANFDMessage.ext` (29-bit) | `CanMsg.ext_id` | already supported |
| `Normal20B` mode | no equivalent | gap |
| `mCLKOPin` / CLKO divider | not exposed | gap |
| Dual INT pins (INT0/INT1) | single INT pin | gap |

---

## 17. Frame timing requirements

Battery-Emulator transmits frames on strict intervals:
- Kia 64 FD: 200 ms and 10 s periodic frames, plus a 63-frame startup sequence with
  per-frame delays (0–150 ms).
- MEB: 10 ms, 20 ms, 40 ms, 50 ms, 100 ms, 200 ms, 500 ms, 1 s periodic frames.

These are driven by `millis()` in the Arduino loop — the CAN driver is not involved in
scheduling. The driver only needs to accept frames as fast as the loop submits them and
not block. Non-blocking transmit is the critical requirement.

---

## 18. ISO-TP / UDS requirement

The MEB battery uses ISO-TP (ISO 15765-2) for UDS PID polling and DTC read/clear.
The ISO-TP layer (`Software/src/lib/uds_isotp/`) sits above the CAN driver and calls
`transmit_can_frame_to_interface()` for each CAN frame it needs to send. It does not
interact with the CAN driver directly. No special ISO-TP support is needed from our
driver — it just needs to send and receive individual CAN FD frames reliably.

---

## 19. Safety interlock

`allowed_to_send_CAN` is checked before every transmit. This is Battery-Emulator's
concern. Our driver must not transmit autonomously (keepalives, error frames) in a way
that bypasses this check. Our driver only transmits when `transmit()` is called — no
autonomous TX.

---

## 20. Summary of integration requirements

| # | Requirement | Priority |
|---|---|---|
| IR-01 | Non-blocking transmit returning success/failure | Must |
| IR-02 | Non-blocking receive (`available()` + dequeue) | Must |
| IR-03 | CAN FD frames with BRS (`fdf=true, brs=true`) | Must |
| IR-04 | Classic CAN 2.0B frames on FD chip (`fdf=false`) | Must |
| IR-05 | 11-bit standard IDs | Must |
| IR-06 | 29-bit extended IDs | Must |
| IR-07 | DLC up to 64 bytes (DLC code 0–15) | Must |
| IR-08 | DLC 48 bytes (DLC code 14) specifically | Must |
| IR-09 | Nominal rates: 250 kbps, 500 kbps | Must |
| IR-10 | Data rates: 1 Mbps, 2 Mbps (4× nominal) | Must |
| IR-11 | Oscillator autodetect (20 MHz / 40 MHz) | Must |
| IR-12 | `stop()` / `restart()` lifecycle | Must |
| IR-13 | Bus error query (equivalent to `hasCanErrors()`) | Must |
| IR-14 | ISR self-managed — no ISR callback required from caller | Must |
| IR-15 | Two independent driver instances on same or different SPI buses | Must |
| IR-16 | `configure()` callable again for restart (re-entrant init) | Must |
| IR-17 | 40 MHz oscillator support | Must |
| IR-18 | Normal20B mode (classic CAN on FD chip, no BRS) | Should |
| IR-19 | CLKO output pin configuration and divider | Should |
| IR-20 | Dual INT pin support (INT0 + INT1 as used by T-2CAN) | Should |
