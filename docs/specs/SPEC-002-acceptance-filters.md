# SPEC-002 — Acceptance Filter API

## Status
Done

## Priority
High

## Use cases covered
UC-1 (EV Battery Gateway), UC-2 (Data Logger), UC-3 (UDS Diagnostics)

## Problem
The driver configures exactly one filter (filter 0) as a catch-all (SID/EID mask all zeros,
routed to FIFO2). There is no API to add, remove or configure filters.

On a busy CAN FD bus (VW MEB sends ~15 different IDs every 10–100 ms) the application
receives every frame and must dispatch in software. More critically, for UDS diagnostics the
application needs to receive only the response SID (e.g. 0x7EC) and ignore everything else —
there is currently no way to express this.

The MCP2518FD supports up to 32 filters (CiFLTCON0–7, CiFLTOBJ0–31, CiMASK0–31),
each routable to any FIFO.

## Requirement
- Add `setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext)` to `MCP2518Driver`
  - `index` 0–31 selects the filter slot
  - `id` and `mask` are full 29-bit values (11-bit callers just use the lower 11 bits)
  - `ext` = true sets MIDE=1 (match extended only), false sets MIDE=0 (match both)
  - All matched frames route to FIFO2 (the single RX FIFO)
  - Must be called after `configure()`, before or during normal operation
  - Can be called in normal mode — filter enable/disable does not require config mode
- Add `clearFilter(uint8_t index)` to disable a filter slot
- `configure()` continues to install filter 0 as catch-all by default so existing code is unaffected
- Filter register addresses for slots 1–31 derived from existing `FIFO_CON`-style helpers

## Acceptance criteria
- Configure filter 0 to match only SID 0x7EC (mask 0x7FF, ext=false)
- Transmit SID 0x7EC and SID 0x123 in internal loopback
- `receive()` returns only the 0x7EC frame; 0x123 is not in the FIFO
- Configure filter 0 to match EID 0x1C42017B (mask 0x1FFFFFFF, ext=true) — requires SPEC-001
- Transmit EID 0x1C42017B and EID 0x18DAF101 in internal loopback
- Only 0x1C42017B is received
- All existing loopback assertions still pass with default catch-all filter

## Notes
- Verify CiFLTCON byte layout (FLTEN bit, F0BP routing field) against DS20006027B before writing
- Filter changes in normal mode: check whether FLTEN can be toggled outside config mode per datasheet
- MIDE bit in CiMASK controls whether IDE bit is included in the match — must be set correctly for ext=true filters

## Datasheet findings

### Filter changes do not require Configuration mode (DS20005678E Section 6.1, page 36)
> "The filter must be disabled by clearing the FLTEN bit before changing the Filter or Mask Object.
> The module **does not have to be in Configuration mode**."

Sequence for any filter update (safe in normal mode):
1. Write `0x00` to the filter's byte in CiFLTCONm — disables it
2. Write CiFLTOBJn and CiMASKn
3. Write `(1<<7) | fifo_pointer` to re-enable

### CiFLTCONm register layout (DS20006027B Register 3-32, pages 60–61)
Each CiFLTCONm register covers 4 filters, one byte per filter:
- bit [7]   = FLTENn (enable)
- bits [6:5] = unimplemented
- bits [4:0] = FnBP (FIFO pointer, 0x02 = FIFO2)

Address of the byte for filter n: `0x1D0 + (n/4)*4 + (n%4)`

**Critical:** FnBP can only be modified while FLTENn=0 (DS20006027B Register 3-32 Note 1).
Always disable the filter before writing the routing pointer.

### CiFLTOBJn / CiMASKn address stride (DS20006027B page 13 register summary)
- CiFLTOBJn = `0x1F0 + n*8`
- CiMASKn   = `0x1F4 + n*8`
- Stride is 8 bytes per filter (OBJ then MASK, 4 bytes each)

### CiFLTOBJn bit layout (DS20006027B Register 3-33, page 62)
Identical to T0/R0 message object word:
- bits [28:11] = EID[17:0]
- bits [10:0]  = SID[10:0]
- bit  [30]    = EXIDE (1 = match extended frames only, when MIDE=1)

### CiMASKn bit layout (DS20006027B Register 3-34, page 63)
- bits [28:11] = MEID[17:0]
- bits [10:0]  = MSID[10:0]
- bit  [30]    = MIDE (1 = enforce IDE match via EXIDE; 0 = accept both SID and EID)

### MIDE=0 catch-all behaviour
With MIDE=0 and all mask bits zero, the filter accepts every frame regardless of IDE.
This is how `configFilter()` installs the default catch-all — no special handling needed.
