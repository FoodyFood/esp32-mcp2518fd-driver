# SPEC-001 — 29-bit Extended ID (EID) Support

## Status
Done

## Priority
High

## Use cases covered
UC-1 (EV Battery Gateway), UC-3 (UDS Diagnostics)

## Problem
`CanMsg.sid` is 11-bit only. The TX object word 0 encodes only SID bits [10:0].
The RX decode reads only bits [10:0] from object word 0 and ignores the EID fields.

VW MEB battery uses 29-bit IDs for all ISO-TP traffic:
- TX functional address: `0x1C420017`
- RX physical response: `0x1C42017B`

Without EID support these batteries cannot be communicated with at all.

## Requirement
- `CanMsg` gains `bool ext` (false = 11-bit SID, true = 29-bit EID) and `uint32_t id`
  replacing `uint16_t sid`, carrying the full identifier in both cases
- `transmit()` encodes EID into TX object words 0–1 per DS20006027B Table 3-1:
  - word 0 bits [28:18] = SID[10:0], bits [17:11] = EID[28:22], bits [3:0] = EID[17:14] (SID11 + EID[28:18])
  - word 0 bit 4 = IDE (1 for extended)
  - word 1 bits [31:16] = EID[17:0] upper, bits [15:0] = EID[17:0] lower — per object layout
- `receive()` decodes EID from RX object words and populates `msg.ext` and `msg.id`
- `dlcToLen()` and all existing behaviour unchanged
- Existing callers using 11-bit IDs continue to work with `ext=false`

## Acceptance criteria
- `transmit()` a frame with `ext=true, id=0x1C420017, dlc=8` in internal loopback
- `receive()` returns `ext=true, id=0x1C420017` with all 8 bytes intact
- `transmit()` a frame with `ext=false, id=0x123` — existing loopback test still passes unchanged
- Both verified on hardware via loopback example, all assertions OK

## Notes
- Verify exact TX/RX object word layout against DS20006027B Table 3-1 and 3-2 before writing any code
- `configFilter()` catch-all mask must also be updated to pass EID frames through (MIDE=0 in CiMASK0)
- `CanMsg.sid` field name should be retired — rename to `id`, add `ext` bool

## Datasheet findings

### TX/RX object word layout (DS20006027B Table 3-5 / Table 3-6, pages 66–67)
The spec referenced Table 3-1 and 3-2 — these do not exist. The correct tables are:
- **Table 3-5** — Transmit Message Object (TXQ and TX FIFO)
- **Table 3-6** — Receive Message Object

Both share the same word 0 (T0/R0) identifier layout:
- bits [31:30] — unimplemented
- bit  [29]    — SID11 (12-bit FD base format extension, not used for 29-bit EID)
- bits [28:11] — EID[17:0]
- bits [10:0]  — SID[10:0]

For a 29-bit extended ID the mapping is:
- `SID[10:0]` = `id >> 18`  (top 11 bits of the 29-bit ID)
- `EID[17:0]` = `id & 0x3FFFF`  (bottom 18 bits)

Word 1 (T1/R1) control bits:
- bit [7] = FDF, bit [6] = BRS, bit [5] = RTR, bit [4] = IDE, bits [3:0] = DLC
- IDE=1 signals an extended frame to the controller

### RX data offset
R2 (timestamp word) is only present when `CiFIFOCONm.RXTSEN=1`, which we do not set.
Data therefore starts at `addr + 8` for both SID and EID frames — same offset as before.

### Catch-all filter and EID frames
`CiMASK0 = 0x00000000` means MIDE=0, which causes the filter to match both standard
and extended frames regardless of IDE. No change was needed to `configFilter()` for EID
frames to pass through — the existing catch-all already accepted them.

### API design decision
`CanMsg.sid` (uint16_t) was replaced by `CanMsg.id` (uint32_t) + `CanMsg.ext` (bool).
Keeping `sid` alongside `id` was rejected: two fields expressing the same thing when
`ext=false` violates SRP on the struct and creates ambiguity at every call site.
All fields default to zero so existing callers compile and behave correctly without modification.
