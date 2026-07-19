# SPEC-003 — Bus Error Detection and TX Error Detail

## Status
Done

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
- Disconnect the bus (no second node, MODE_NORMAL): `transmit()` returns `CanTxResult::NoAck` or `CanTxResult::BusError`
  - With no bus connected (floating CANH/CANL), the chip detects a bit error on the first bit and sets TXERR → `BusError`
  - With bus connected but no second node (termination present), the chip exhausts retries with no ACK and sets TXABT → `NoAck`
  - Both indicate failure; callers should treat both as "frame not delivered"
- After failed TX: `getErrors().tec` > 0
- After repeated failed TX: all calls return non-OK; `getErrors()` is readable without crash
- Note: `txWarning` / `busOff` thresholds are not reliably testable in automated single-board tests because the chip auto-recovers from bus-off and resets CiTREC before the next read
- `hasErrors()` returns true once TEC >= 96 (verifiable in two-node scenarios where TEC accumulates steadily)
- All existing loopback assertions updated to check `CanTxResult::OK` instead of `true`

## Notes
- `transmit()` return type change is a breaking API change — all call sites in examples must be updated
- CiBDIAG0 bits: DBIT (data phase bit error), ABIT (arbitration bit error), BERR, SERR, FERR, B0ERR, B1ERR — verify field positions against DS20006027B before writing
- CiTREC layout: TEC[7:0] at bits 7:0, REC[7:0] at bits 23:16, TXWARN/RXWARN/TXBP/RXBP/TXBO flags at bits 8–12 — verify against datasheet
- busOff recovery requires entering and exiting config mode — document this in the API but do not implement auto-recovery

## Datasheet findings

### CiTREC — 0x034 (DS20006027B Register 3-20, page 43)
- bits 7:0   — REC[7:0]: receive error counter
- bits 15:8  — TEC[7:0]: transmit error counter
- bit 16     — EWARN: TEC or REC >= 96 (error warning threshold)
- bit 17     — RXWARN: REC >= 96
- bit 18     — TXWARN: TEC >= 96
- bit 19     — RXBP: REC >= 128 (error passive)
- bit 20     — TXBP: TEC >= 128 (error passive)
- bit 21     — TXBO: TEC > 255 (bus-off)
- bits 31:22 — unimplemented, read as 0
- NOTE: TXBO POR value is 1 (R-1 in datasheet) — reads as 1 at startup before any error occurs; only meaningful after the chip has been in Normal mode
- NOTE: CiTREC, CiBDIAG0 and CiBDIAG1 are **reset when exiting Configuration mode** — do not read them immediately after configure(); they are only valid after the chip has been in Normal mode

### CiBDIAG0 — 0x038 (DS20006027B Register 3-21, page 44)
- bits 7:0   — NRERRCNT[7:0]: nominal receive error count (increments on every error, never decrements, cleared by writing 0)
- bits 15:8  — NTERRCNT[7:0]: nominal transmit error count
- bits 23:16 — DRERRCNT[7:0]: data phase receive error count
- bits 31:24 — DTERRCNT[7:0]: data phase transmit error count
- These counters differ from CiTREC: they only increment, never decrement, and can be cleared by writing 0

### CiBDIAG1 — 0x03C (DS20006027B Register 3-22, page 45)
- bit 16 — NBIT0ERR: nominal bit 0 error (transmitter sent recessive, saw dominant)
- bit 17 — NBIT1ERR: nominal bit 1 error (transmitter sent dominant, saw recessive)
- bit 18 — NACKERR: transmitted message was not acknowledged — **this is the NoAck indicator**
- bit 19 — NFORMERR: fixed-format part of received frame has wrong format
- bit 20 — NSTUFFERR: more than 5 equal bits in sequence
- bit 21 — NCRCERR: CRC mismatch on received message
- bit 22 — unimplemented
- bit 23 — TXBOERR: device went to bus-off (and auto-recovered) — set on exit from bus-off
- bit 24 — DBIT0ERR: data phase bit 0 error
- bit 25 — DBIT1ERR: data phase bit 1 error
- bit 26 — unimplemented
- bit 27 — DFORMERR: data phase form error
- bit 28 — DSTUFFERR: data phase stuff error
- bit 29 — DCRCERR: data phase CRC error
- bit 30 — ESI: ESI flag of received CAN FD message was set
- bit 31 — DLCMM: DLC mismatch (DLC larger than PLSIZE of FIFO element)
- bits 15:0 — EFMSGCNT[15:0]: error-free message counter
- Flags and counter are cleared by writing 0 to the register

### CiRXOVIF — 0x028 (DS20006027B Register 3-16, page 39)
- bits 31:1 — RFOVIF[31:1]: bit N set = FIFO N overflowed (bit 0 reserved, maps to TXQ)
- Read-only; cleared by reading CiFIFOSTAm.RXOVIF and then setting UINC
- For our driver (FIFO2=RX): check bit 2 of CiRXOVIF

### TX error classification from CiFIFOSTA1 (already used by transmit())
- TXABT (bit 7): message aborted after exhausting retransmission attempts — maps to **NoAck** when no second node present
- TXERR (bit 5): bus error detected during transmission — maps to **BusError**
- These are HS/C bits (hardware-set, cleared by hardware on next TX attempt start)
- The existing transmit() already reads these; CanTxResult just formalises the distinction

### Bus-off behaviour (DS20005678E Section 11.1, page 69)
- Bus-off recovery is **automatic** — chip counts 128 idle conditions then exits bus-off
- On entering bus-off: chip sets FRESET on all TX FIFOs (clears pending TX)
- On exiting bus-off: CiBDIAG1.TXBOERR is set and CiTREC is reset
- Application is notified via CERRIF on both entry and exit
- No manual intervention required for recovery; but application should re-queue messages after FRESET clears them

### CERRIF threshold crossings (DS20005678E Section 10.5.3, page 63)
- CERRIF is set each time TEC or REC crosses a threshold: 96 (warning), 128 (passive), 256 (bus-off)
- It is also set on exit from bus-off
- Must be cleared by application; will remain clear until next threshold crossing
