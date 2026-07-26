# SPEC-008 — Classic CAN Mode on FD Chip (Normal20B)

## Status
Done

## Context
Drawn from [`docs/use_cases/uc-dala-battery-emulator.md`](../use_cases/uc-dala-battery-emulator.md) IR-18.

Battery-Emulator has a user-configurable flag `use_canfd_as_can`. When true, the MCP2518FD
is put into `Normal20B` mode — it operates as a classic CAN 2.0B controller. No FD frames
are transmitted or received, and BRS is never set. This is used when the battery protocol
is classic CAN but the only available hardware has an MCP2518FD chip.

Our driver currently has no equivalent. `MODE_NORMAL` always enables FD. This spec adds a
`MODE_CLASSIC` that configures the chip as a plain CAN 2.0B controller.

## Datasheet findings
- REQOP=110 (0x06) = Normal CAN 2.0 mode — confirmed DS20006027B page 27, REFMANUAL page 10.
- OPMOD=110 is the readback value when the chip is in Normal CAN 2.0 mode — same encoding.
- The chip ignores FDF, BRS and ESI bits in TX objects and transmits them as '0' (REFMANUAL page 10).
  The chip itself will not send FD frames in this mode, but we guard at the API level (AC-5) to
  prevent the caller from relying on silent coercion.
- The chip will send error frames if CAN FD frames are detected on the bus (REFMANUAL page 10).
- Data phase bit timing registers (CiDBTCFG, CiTDC) are irrelevant in Normal CAN 2.0 mode;
  we write nominal timing to both to keep the register state consistent.
- MODE_CLASSIC = 6 was already defined in registers.h — value confirmed correct.
- calcBitTiming() requires dataBps != 0; in classic mode we pass nominalBps for both to satisfy
  the function without adding a special-case path inside it.

## Acceptance criteria

### AC-1 — New mode constant
`MODE_CLASSIC` is defined and accepted by `configure()` and `configureRaw()`.

### AC-2 — Chip enters Normal20B
When `configure(nominal, 0, MODE_CLASSIC)` is called, the chip enters `Normal20B` mode
(REQOP=0b110 per the datasheet). `getMode()` returns `MODE_CLASSIC`.

### AC-3 — Data rate parameter ignored
The data rate argument is ignored in `MODE_CLASSIC`. Passing any value (including 0) does
not cause `RATE_NOT_ACHIEVABLE`. The data phase bit timing registers are left at reset
defaults or set to match the nominal rate.

### AC-4 — Classic frames transmit and receive
In `MODE_CLASSIC`, `transmit()` with `fdf=false, brs=false` succeeds. `receive()` returns
frames with `fdf=false`. Verified in internal loopback.

### AC-5 — FD frames rejected on transmit
In `MODE_CLASSIC`, `transmit()` with `fdf=true` returns `CanStatus::INVALID_MODE` (or
equivalent) without touching the chip. No malformed frame is placed on the bus.

### AC-6 — Two-node classic CAN
Two boards, both in `MODE_CLASSIC` at 500 kbps, exchange frames successfully. Verified
on real hardware (COM4 + COM3).

### AC-7 — No regression
All existing single_node, id_filter and two_node assertions pass after this change.

## API change
```cpp
// New mode constant alongside existing MODE_NORMAL, MODE_LISTEN, etc.
#define MODE_CLASSIC  0x06   // value TBC from datasheet

// configure() data rate is ignored when mode == MODE_CLASSIC
CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, ...);
```

## Test harness
Add a `classic_mode` block to `tests/integration/single_node/src/main.cpp`:
- Configure `MODE_CLASSIC` at 500 kbps
- Transmit a classic frame, receive it in loopback, assert `fdf=false`
- Attempt to transmit an FD frame, assert rejection

Add a `classic_two_node` block to `tests/integration/two_node/src/main.cpp`:
- Both nodes in `MODE_CLASSIC`, exchange 10 frames, assert all received

## Notes
- The data rate argument convention when calling `configure()` in classic mode needs a
  decision: accept 0, accept any value silently, or require the caller to pass the nominal
  rate again. Document the chosen behaviour clearly in the API header.
- `setDataRate()` must return an error or no-op in `MODE_CLASSIC` — document this.
