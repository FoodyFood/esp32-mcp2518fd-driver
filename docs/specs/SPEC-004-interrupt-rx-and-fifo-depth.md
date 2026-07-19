# SPEC-004 — Interrupt-driven RX and Configurable FIFO Depth

## Status
In Progress

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

## Datasheet findings

### INT pin (DS20006027B Table 1-1, page 4; Section 6.0.1, page 76)
- INT is an **active-low output** pin — asserted (driven low) when any enabled interrupt is pending
- INT is asserted on any interrupt in CiINT where the corresponding xIF and xIE bits are both set
- All three interrupt pins (INT, INT0, INT1) are active low — confirmed
- INT pin is open-drain by default (INTOD=0 = push/pull at POR); no external pull-up required for push/pull mode
- Interrupts are **persistent**: the INT pin stays asserted until the condition is cleared inside the chip (e.g. UINC on the RX FIFO drains the frame)
- To use INT for RX notification: set CiINT.RXIE (bit 17) and enable TFNRFNIE on FIFO2

### CiINT — 0x01C (DS20006027B Register 3-14, page 36)
- bit 17 — RXIE: Receive FIFO Interrupt Enable — when set, INT pin asserts when CiINT.RXIF is set
- bit 1  — RXIF: Receive FIFO Interrupt Flag — set when any enabled RX FIFO interrupt is pending
- RXIF is read-only; it is the OR of all enabled FIFO RFNIF flags; cleared automatically when the FIFO condition clears
- To enable: write RXIE=1 to CiINT byte 2 (address 0x01E)

### CiFIFOCONm FSIZE field (DS20006027B Register 3-29, page 54; Table 3-2, page 11)
- bits 28:24 — FSIZE[4:0]: FIFO size in message objects
  - 00000 = 1 message object
  - 00001 = 2 message objects
  - ...
  - 11111 = 32 message objects
- FSIZE can only be written in Configuration mode
- Current driver hardcodes FSIZE=4 (value 3 in the field = 4 slots)

### CiFIFOCONm TFNRFNIE bit (DS20006027B Register 3-29, page 54)
- bit 0 — TFNRFNIE: TX/RX FIFO Not Full/Not Empty Interrupt Enable
  - For RX FIFO (TXEN=0): enables interrupt when FIFO is not empty (TFNRFNIF set)
  - This is the per-FIFO interrupt enable that feeds into CiINT.RXIF
  - Must be set on FIFO2 for the INT pin to assert on frame arrival

### RAM layout calculation (DS20005678E Section 3.5, page 23)
- Formula: SFIFO(i) = NElements(i) × (rxts(i) + 8 + PayLoad(i))
- With PLSIZE_64, no timestamp (rxts=0): each slot = 8 + 64 = 72 bytes
- FIFO1 (TX, depth=4): 4 × 72 = 288 bytes, starts at RAM 0x400, ends at 0x520
- FIFO2 (RX) starts at 0x520
- Total RAM = 2048 bytes; available for FIFO2 = 2048 - 288 = 1760 bytes
- Max FIFO2 depth at PLSIZE_64 = floor(1760 / 72) = **24 slots** (not 27 as spec originally stated)
- The spec's default of 16 is safe; 24 is the hard maximum at PLSIZE_64
- Clamping to 24 (not 32) is the correct upper bound for this driver's fixed FIFO1 depth=4

### Interrupt enable sequence
1. In config mode: set FSIZE on FIFO2, set TFNRFNIE on FIFO2 (bit 0 of CiFIFOCON2 byte 0)
2. In config mode: set RXIE in CiINT (bit 17 = byte 2 bit 1 of CiINT at 0x01C)
3. Exit config mode — INT pin will assert (go low) when a frame arrives in FIFO2
4. On ESP32: attachInterrupt(intPin, isr, FALLING) — ISR sets volatile flag only, no SPI
5. In receive(): after draining FIFO with UINC, INT pin deasserts automatically
