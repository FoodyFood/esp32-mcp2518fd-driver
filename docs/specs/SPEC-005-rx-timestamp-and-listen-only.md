# SPEC-005 — RX Timestamp and Listen-Only Mode Validation

## Status
Pending

## Priority
Medium

## Use cases covered
UC-1 (EV Battery Gateway), UC-2 (Data Logger)

## Problem
Two related gaps addressed together because both serve passive monitoring / logging:

**RX timestamp (G6):** The MCP2518FD has a free-running 32-bit time base counter (CiTBC)
and per-FIFO receive timestamping (RXTSEN bit in CiFIFOCONm). When enabled, each received
message object includes a 32-bit timestamp captured at SOF. Battery-Emulator works around
this by calling `millis()` after receive — coarse and subject to polling jitter.

**Listen-only mode (G10):** `MODE_LISTEN` (REQOP=3) is defined as a constant but has never
been tested on a real bus. In listen-only mode the chip receives all frames without
transmitting ACK bits — essential for a passive logger or bus analyser that must not disturb
the bus. The mode needs to be validated and documented.

## Requirement

### RX Timestamp
- `CanMsg` gains `uint32_t timestamp` field (0 when timestamping not enabled)
- `configure()` gains optional `bool enableTimestamp` parameter (default false)
  - When true: enables CiTBC free-running counter (CiTSCON TBCEN=1) and sets RXTSEN in
    FIFO2 CON register
  - Timestamp resolution: 1 TQ at the nominal bit rate (e.g. 8 µs at 125 kbps / 20 MHz)
- `receive()` reads the timestamp word from the RX message object when RXTSEN is set and
  populates `msg.timestamp`; leaves it 0 otherwise
- `getFsys()` and the configured nominal rate allow the caller to convert TQ counts to µs

### Listen-Only Mode Validation
- Validate `MODE_LISTEN` on real hardware with two nodes:
  - Node A in `MODE_LISTEN`, Node B in `MODE_NORMAL` transmitting frames
  - Node A receives frames correctly
  - Node A does NOT transmit ACK (verified: Node B sees no ACK error, bus traffic unaffected)
  - Node A `transmit()` returns `CanTxResult::NoAck` (or is documented as unsupported in listen mode)
- Document validated behaviour in `mcp2518fd_can.h` and `docs/context.md`
- Add a `listen_only` example or extend `bus_monitor` with a listen mode

## Acceptance criteria
- `configure(500000, 2000000, MODE_NORMAL, 16, true)` — CiTBC increments, RXTSEN set in FIFO2
- Transmit a frame in internal loopback; `receive()` returns `msg.timestamp > 0`
- Transmit two frames 1 ms apart; `msg.timestamp` difference corresponds to ~1 ms in TQ counts
- `MODE_LISTEN` on real bus: Node A receives all frames from Node B, Node B reports no errors
- All existing loopback assertions pass (timestamp=0 when not enabled)

## Notes
- CiTSCON register (0x014): TBCEN bit enables the time base counter — verify bit position against DS20006027B
- CiTBC register (0x010): 32-bit free-running counter value
- RXTSEN is bit 5 of CiFIFOCONm — already defined as `FIFOCON_RXTSEN` in registers.h
- RX message object layout with timestamp: words 0–1 are T0/T1 (ID + control), words 2–N are
  payload, final word is timestamp — verify exact position against DS20006027B Table 3-2
- In listen-only mode the chip cannot transmit; `transmit()` should return `CanTxResult::NoAck`
  immediately without attempting to write to the TX FIFO
