# SPEC-006 — Stop, Restart and Sleep/Wake Lifecycle

## Status
Done

## Priority
Medium

## Use cases covered
UC-1 (EV Battery Gateway)

## Problem
There is no way to pause and resume the chip. Battery-Emulator calls `canfd->end()` and
`canfd->begin()` during BMS reset sequences — the BMS is power-cycled and the CAN bus goes
silent for several seconds; the gateway must stop transmitting keepalive frames, wait, then
resume cleanly without a full `configure()` call.

`MODE_SLEEP` is defined as a constant but is not documented, tested, or reachable via any
public API method.

## Requirement

### stop() / restart()
- `stop()` — enters `MODE_CONFIG` (halts TX/RX), does not reset the chip or lose timing config
  - Safe to call at any time; idempotent if already stopped
- `restart()` — returns to the mode that was active before `stop()` was called
  - Restores the previous OPMOD (normal, external loopback, etc.)
  - Does not re-run `configure()` — timing and FIFO config are preserved across stop/restart

### sleep() / wake()
- `sleep()` — requests `MODE_SLEEP` (REQOP=1); chip enters low-power state
  - Documents that the INT pin or a bus wake-up event is required to exit sleep
  - Returns `CanStatus::OK` when OPMOD confirms sleep, `CanStatus::MODE_TIMEOUT` otherwise
- `wake()` — writes REQOP=MODE_CONFIG to exit sleep, then restores previous mode
  - On ESP32 the INT pin (GPIO 34) can be used as a wake source — document but do not
    implement the ESP32 deep-sleep integration (out of scope)

## Acceptance criteria
- `configure(500000, 2000000, MODE_NORMAL)` then `stop()` — `getMode()` returns `MODE_CONFIG`
- `restart()` — `getMode()` returns `MODE_NORMAL`
- `transmit()` after `stop()` returns `CanTxResult::FifoFull` or is documented as invalid
- `sleep()` — `getMode()` returns `MODE_SLEEP`
- `wake()` — `getMode()` returns `MODE_NORMAL` (or whatever mode was active before sleep)
- All existing loopback assertions pass after a stop/restart cycle

## Notes
- `stop()` is just `setMode(MODE_CONFIG)` with the previous mode saved — minimal implementation
- `restart()` is just `setMode(savedMode)` — equally minimal
- Sleep wake-up on MCP2518FD: the chip wakes on bus activity or on the INT pin being driven;
  verify wake-up sequence (OSC ready check) against DS20006027B before implementing `wake()`
- Do not auto-call `restart()` inside `wake()` without confirming OSCREADY first

## Datasheet findings
- **OSCDIS is bit 2** of OSC byte 0 (DS20006027B Register 3-1, page 17). Not bit 3.
  Layout: `— CLKODIV[1:0] SCLKDIV LPMEN OSCDIS — PLLEN` → bits 7..0.
- **Sleep handshake**: poll `OPMOD == MODE_CONFIG` AND `OSC.OSCDIS == 1` to confirm sleep
  entered (DS20006027B page 26 note 2, ref manual page 11).
- **Sleep exit**: clear `OSC.OSCDIS` (bit 2 of OSC byte 0). The chip re-enables the oscillator
  and transitions automatically to Configuration mode. Writing REQOP=CONFIG does NOT exit sleep.
  (DS20005678E page 11 section 2.4.1; ref manual Figure 2-1 shows `WAKIF or OSC.OSCDIS=0`.)
- **OSCREADY after wake**: oscillator stabilisation from sleep is up to 3 ms (DS20006027B page 79
  TOSCSLP). Poll bit 10 of OSC register before restoring the previous mode.
- **configure() must handle sleep**: if the chip is sleeping when configure() is called, OSCDIS
  must be cleared before reset() is issued, otherwise detectFsys() reads OSCREADY=0 and returns 0.
- **Normal sleep vs LPM**: REQOP=001 with LPMEN=0 (reset default) enters normal sleep — register
  and RAM contents preserved. LPMEN=1 enters LPM — logic powered down, contents lost, wake via
  nCS assertion only. This driver uses normal sleep only.
