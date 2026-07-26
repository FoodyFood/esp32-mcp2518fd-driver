# SPEC-005 — RX Timestamp and Listen-Only Mode Validation

## Status
Done

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
- RX message object layout with timestamp (DS20006027B Table 3-6 / ref manual Table 7-1):
  R0 (+0) = SID/EID, R1 (+4) = FILHIT/ESI/FDF/BRS/RTR/IDE/DLC,
  R2 (+8) = RXMSGTS (timestamp, only when RXTSEN=1), R3 (+12) = payload bytes 0-3, ...
  The timestamp word comes BEFORE the payload, not after.
- In listen-only mode the chip cannot transmit; `transmit()` should return `CanTxResult::NoAck`
  immediately without attempting to write to the TX FIFO
- CiTSCON must be written AFTER setMode() exits config mode — writing TBCEN in config mode
  does not survive the mode transition (register resets on config-mode exit)
- TBCPRE=0 (default): TBC increments every 1 FSYS clock = 50 ns at 20 MHz
- Slot size with RXTSEN=1: 4(R0)+4(R1)+4(R2)+64(payload) = 76 bytes; UA advances by 76

## Datasheet findings
- DS20006027B Table 3-6 / ref manual Table 7-1: RX message object word order is
  R0 (SID/EID), R1 (control), R2 (RXMSGTS, only when RXTSEN=1), then payload.
  The timestamp is at offset +8 from the slot base, payload starts at +12 (with RXTSEN)
  or +8 (without RXTSEN). This is the opposite of what the spec originally assumed.
- CiTSCON.TBCEN (bit 16) must be written after setMode() completes — the register
  resets when the chip exits Configuration mode, so writing it during configFifos()
  (which runs in config mode) has no effect.
- Ref manual page 58: "The TBC has to be disabled before writing to CiTBC by clearing
  CiTSCON.TBCEN." TBCPRE=0 gives 1-clock resolution (50 ns at 20 MHz).
- Listen Only mode (ref manual page 12): "TXREQ bits will be ignored. No error flags or
  Acknowledge signals are sent." The chip receives normally but transmits nothing.
  transmit() must detect MODE_LISTEN and return NoAck immediately — confirmed on hardware.
