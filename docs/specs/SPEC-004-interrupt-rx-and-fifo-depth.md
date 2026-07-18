# SPEC-004 — Interrupt-driven RX and Configurable FIFO Depth

## Status
Pending

## Priority
Medium

## Use cases covered
UC-1 (EV Battery Gateway), UC-2 (Data Logger), UC-4 (Inverter Interface)

## Problem
Two related gaps addressed together because they both protect against frame loss:

**Interrupt-driven RX (G4):** The driver is polling-only. On a busy CAN FD bus (VW MEB
sends frames every 10 ms on ~15 IDs; inverter telemetry at 10 ms) frames can arrive faster
than the main loop polls. Battery-Emulator uses `canfd->isr()` via a GPIO interrupt on the
MCP2518FD INT pin (GPIO 34 on this hardware) to ensure no frame is missed.

**Configurable FIFO depth (G8):** FSIZE is hardcoded to 4 in `configFifos()`. With a 4-slot
FIFO and polling-only RX, a burst of 5 back-to-back CAN FD frames overflows before the poll
loop runs. The MCP2518FD supports FSIZE 1–32 per FIFO.

These two are coupled: a deeper FIFO buys time between polls; an interrupt callback eliminates
the polling gap entirely. Both are needed for reliable high-traffic operation.

## Requirement

### Configurable FIFO depth
- `configure()` gains an optional `uint8_t rxFifoDepth` parameter (default 16)
  - Valid range 1–32; clamped silently to 32 if exceeded
  - Applied to FIFO2 FSIZE field during `configFifos()`
  - RAM budget: each FIFO slot = 8 bytes header + PLSIZE payload (64 bytes) = 72 bytes
    × 32 slots = 2304 bytes — fits within the 2KB RAM only at smaller PLSIZE; at PLSIZE_64
    max safe depth is 27 slots (27 × 72 = 1944 bytes). Document this constraint.

### Interrupt-driven RX
- `MCP2518Driver` constructor gains optional `int8_t intPin` parameter (default -1 = polling)
- When `intPin >= 0`, `configure()` attaches an ISR to the pin (FALLING edge, INT pin is
  active-low) that sets a volatile flag `mRxPending`
- `available()` returns true if `mRxPending` is set OR FIFOSTA_TFNRFNIF is set (belt-and-braces)
- `receive()` clears `mRxPending` after draining the FIFO
- The ISR itself does nothing except set the flag — no SPI inside ISR
- When `intPin == -1` behaviour is identical to today (pure polling)

## Acceptance criteria
- `configure(500000, 2000000, MODE_NORMAL, 16)` — FIFO2 FSIZE reads back as 16 in config mode
- Send 16 frames back-to-back in internal loopback; all 16 received without overflow
- Send 17 frames — 17th triggers RXOVIF (verified via `getErrors().rxOverflow` from SPEC-003)
- With `intPin=34`: transmit one frame, confirm `available()` returns true within 1 ms without
  polling (verified by checking `mRxPending` flag is set before `available()` is called)
- All existing loopback assertions pass with default depth=16

## Notes
- INT pin is GPIO 34 on this hardware (input-only, no pull-up needed — MCP2518FD drives it)
- FIFO2 FSIZE field is bits [28:24] of CiFIFOCON2 — verify against DS20006027B
- RAM layout: FIFO1 (TX) starts at 0x400; FIFO2 (RX) starts immediately after FIFO1.
  With FIFO1 depth=4 and PLSIZE_64: FIFO1 = 4 × 72 = 288 bytes, FIFO2 starts at 0x520.
  Max FIFO2 depth at PLSIZE_64 = (2048 - 288) / 72 = 24 slots. Document this.
- `attachInterrupt()` on ESP32 requires the ISR to be IRAM_ATTR — mark accordingly
