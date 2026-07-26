# SPEC-010 — Dual INT Pin Support (INT0 / INT1)

## Status
Pending

## Context
Drawn from [`docs/use_cases/uc-dala-battery-emulator.md`](../use_cases/uc-dala-battery-emulator.md) IR-20.

The LilyGo T-2CAN FD board wires the MCP2518FD's INT0 and INT1 pins to separate GPIOs
rather than using the combined INT pin. The ACAN2517FD library constructor accepts all
three: `ACAN2517FD(cs, spi, int_pin, int0_pin, int1_pin)` and attaches ISRs to whichever
are valid (not 255).

Our driver constructor currently accepts a single INT pin. On the T-2CAN FD board, INT is
`GPIO_NUM_NC` (not connected) and only INT0 and INT1 are wired. Without this spec, the
driver cannot attach an ISR on that board and falls back to polling — or fails to receive
interrupts entirely.

## Datasheet findings
_To be filled in during implementation after PDF verification._

Key questions to verify from datasheet:
- What events are routed to INT vs INT0 vs INT1?
- Can INT0 alone be used as a general-purpose RX-ready interrupt?
- Is INT0 sufficient to replace INT for our use case (RX FIFO not empty)?

## Acceptance criteria

### AC-1 — Constructor accepts optional INT0 / INT1 pins
The `MCP2518Driver` constructor gains optional `int0Pin` and `int1Pin` parameters,
both defaulting to `GPIO_NUM_NC` / -1.

```cpp
MCP2518Driver(SPIClass& spi, uint8_t csPin,
              int8_t intPin  = -1,
              int8_t int0Pin = -1,
              int8_t int1Pin = -1);
```

Existing callers that pass only `intPin` are unaffected.

### AC-2 — ISR attached to whichever pin is valid
At `configure()` time, the driver attaches an ISR to the first valid pin in priority
order: `intPin`, then `int0Pin`, then `int1Pin`. If none are valid, the driver operates
in polling mode (existing behaviour).

### AC-3 — RX interrupt fires on INT0
On hardware where only INT0 is wired, frame arrival triggers the ISR via INT0.
`available()` returns true immediately after a frame arrives without requiring a poll
loop. Verified on real hardware or confirmed via register readback that INT0 is
configured to assert on RX FIFO not-empty.

### AC-4 — Existing single-INT behaviour unchanged
On hardware with a single INT pin (our current board, Stark CMR), behaviour is identical
to before this spec. No regression in single_node, id_filter or two_node.

### AC-5 — No regression
All existing single_node, id_filter and two_node assertions pass after this change.

## Notes
- The MCP2518FD has a flexible interrupt routing system. Verify from the datasheet which
  interrupt sources are available on INT0 vs INT1 vs INT before deciding which pin to
  attach the ISR to.
- If INT0 and INT1 serve different interrupt sources (e.g. INT0 = RX, INT1 = TX), it may
  be worth attaching both. Keep the implementation minimal — only attach what is needed
  for `available()` to work correctly.
- The T-2CAN FD hardware is needed to fully verify AC-3. If unavailable, verify by
  confirming the INT0 ISR fires in a loopback test on our current hardware with INT0
  wired to a spare GPIO.
