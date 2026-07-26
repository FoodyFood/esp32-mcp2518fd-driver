# SPEC-010 — Dual INT Pin Support (INT0 / INT1)

## Status
Done

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

Verified against DS20006027B (datasheet) page 18 (IOCON register) and page 76 (I/O configuration).

- **INT** (pin 4): asserted on any interrupt in CiINT (xIF & xIE) — the combined interrupt.
- **INT1/GPIO1** (pin 8): RX interrupt — asserted when `CiINT.RXIF & RXIE` are set. Active as interrupt when IOCON.PM1=0.
- **INT0/GPIO0** (pin 9): TX interrupt — asserted when `CiINT.TXIF & TXIE` are set. Active as interrupt when IOCON.PM0=0.
- Reset default: PM1=1, PM0=1 (both pins are GPIO by default — must explicitly clear PM1 to activate INT1).
- **INT0 cannot be used for RX-ready** — it is TX-only. Only INT1 or INT serve RX.
- IOCON register address: `REG_IOCON` = 0xE04. PM1 is bit 25 (byte 3, bit 1). PM0 is bit 24 (byte 3, bit 0).
- IOCON **must be written byte-by-byte** using single data byte SFR WRITE (DS20006027B page 70 note, page 19 note 2).
- To activate INT1 as RX interrupt: `write8(REG_IOCON + 3, read8(REG_IOCON + 3) & ~(1u << 1))` — clears PM1.
- RXIE in CiINT (byte 2, bit 1) is already set by `configFifos()` when any int pin is active — no change needed there.
- INT0 is not useful for our use case (RX FIFO not empty). It is skipped for ISR attachment.

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
