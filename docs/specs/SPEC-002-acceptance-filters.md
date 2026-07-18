# SPEC-002 — Acceptance Filter API

## Status
Pending

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
