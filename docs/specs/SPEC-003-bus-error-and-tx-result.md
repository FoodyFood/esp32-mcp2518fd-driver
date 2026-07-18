# SPEC-003 — Bus Error Detection and TX Error Detail

## Status
Pending

## Priority
High

## Use cases covered
UC-1 (EV Battery Gateway), UC-4 (Inverter Interface), UC-7 (Self-Test)

## Problem
Two related gaps, addressed together because they share the same diagnostic registers:

**Bus error detection (G3):** CiTREC (transmit/receive error counters) and CiBDIAG0/1
(bus diagnostic) are not exposed. Battery-Emulator calls `hasCanErrors()` after every
receive batch and sets a `can_2518_bus_error` flag. Without this, a disconnected bus,
missing termination, or a babbling node is invisible to the application.

**TX error detail (G9):** `transmit()` returns `bool`. When it returns false the caller
cannot tell whether the frame was never ACKed (other node absent), a bus error occurred,
or the FIFO was full. Battery-Emulator tracks `can_2518_send_fail` separately from
`can_2518_bus_error` for exactly this reason.

## Requirement

### CanError struct
```cpp
struct CanError {
    uint8_t  tec;        // transmit error counter (CiTREC bits [7:0])
    uint8_t  rec;        // receive error counter  (CiTREC bits [23:16])
    bool     txWarning;  // TEC >= 96
    bool     rxWarning;  // REC >= 96
    bool     txPassive;  // TEC >= 128
    bool     rxPassive;  // REC >= 128
    bool     busOff;     // TEC > 255 — node is bus-off
    bool     rxOverflow; // at least one RX FIFO overflowed since last call
};
```

### CanTxResult enum
```cpp
enum class CanTxResult : uint8_t {
    OK,        // frame transmitted and ACKed
    NoAck,     // chip retried 3x, no ACK received (other node absent or bus disconnected)
    BusError,  // TXERR set — bit error, stuff error, etc.
    FifoFull,  // TX FIFO had no space (TFNRFNIF was clear)
};
```

### API additions to MCP2518Driver
- `CanError getErrors()` — reads CiTREC, CiBDIAG0, CiRXOVIF; clears sticky overflow flag
- `bool hasErrors()` — returns true if TEC or REC >= 96, or busOff, or rxOverflow; cheap poll
- `transmit()` return type changes from `bool` to `CanTxResult`

## Acceptance criteria
- In internal loopback: `transmit()` returns `CanTxResult::OK`, `hasErrors()` returns false
- Disconnect the bus (no second node, MODE_NORMAL): `transmit()` returns `CanTxResult::NoAck`
- After no-ACK: `getErrors().tec` > 0, `getErrors().busOff` eventually true after repeated attempts
- `hasErrors()` returns true once TEC >= 96
- All existing loopback assertions updated to check `CanTxResult::OK` instead of `true`

## Notes
- `transmit()` return type change is a breaking API change — all call sites in examples must be updated
- CiBDIAG0 bits: DBIT (data phase bit error), ABIT (arbitration bit error), BERR, SERR, FERR, B0ERR, B1ERR — verify field positions against DS20006027B before writing
- CiTREC layout: TEC[7:0] at bits 7:0, REC[7:0] at bits 23:16, TXWARN/RXWARN/TXBP/RXBP/TXBO flags at bits 8–12 — verify against datasheet
- busOff recovery requires entering and exiting config mode — document this in the API but do not implement auto-recovery
