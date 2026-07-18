# SPEC-001 — 29-bit Extended ID (EID) Support

## Status
Pending

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
