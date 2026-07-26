# SPEC-009 — CLKO Output Pin Configuration

## Status
Pending

## Context
Drawn from [`docs/use_cases/uc-dala-battery-emulator.md`](../use_cases/uc-dala-battery-emulator.md) IR-19.

The LilyGo T-2CAN FD board uses the first MCP2518FD's CLKO output to clock the second
MCP2518FD chip. The ACAN2517FD library exposes this via `settings->mCLKOPin` with a
divider value. Our driver currently writes the OSC register at startup but does not expose
CLKO configuration — the CLKO pin is left at its reset default (disabled or divide-by-10,
TBC from datasheet).

Without CLKO configured correctly on the first chip, the second chip has no clock source
and will fail to initialise on the T-2CAN FD board.

## Datasheet findings
_To be filled in during implementation after PDF verification._

Key registers to verify:
- OSC register (0xE00) — CLKODIV field, CLKOEN bit
- Confirm reset default of CLKODIV and CLKOEN

## Acceptance criteria

### AC-1 — New configure() parameter
`configure()` gains an optional `clkoDivider` parameter (default = disabled / 0).
Accepted values map to the CLKODIV field in the OSC register (TBC from datasheet).

### AC-2 — CLKO enabled and correct frequency
When `clkoDivider` is non-zero, the CLKO pin outputs a clock at `FSYS / divider`.
For a 40 MHz oscillator with divider 10: CLKO = 4 MHz. Verified by reading back the
OSC register and confirming CLKODIV and CLKOEN are set as written.

### AC-3 — CLKO disabled by default
When `clkoDivider` is 0 (default), CLKOEN is cleared. The CLKO pin is not driven.
Existing callers that do not pass the parameter are unaffected.

### AC-4 — Second chip clocked from first
On a dual-chip setup where the second chip's oscillator is sourced from the first chip's
CLKO: configure the first chip with `clkoDivider=10` (4 MHz output from 40 MHz), then
configure the second chip with `configure(..., 0, MODE_NORMAL)` (autodetect). The second
chip detects FSYS = 4 MHz and initialises successfully.

This acceptance criterion requires the T-2CAN FD hardware to verify. If that hardware is
not available, verify by reading back OSC register values on a single chip and confirming
CLKODIV is set correctly.

### AC-5 — No regression
All existing single_node, id_filter and two_node assertions pass after this change.

## API change
```cpp
// clkoDivider: 0 = disabled, other values TBC from datasheet CLKODIV field
CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode,
                    uint8_t rxFifoDepth = 8, bool enableTimestamp = false,
                    uint8_t clkoDivider = 0);
```

## Notes
- The CLKODIV encoding in the OSC register must be verified from the datasheet before
  any code is written. Do not assume it is a linear divider value.
- `configureRaw()` should also accept a `clkoDivider` parameter for consistency.
- The autodetect path in `configure()` reads the OSC register to determine FSYS. Confirm
  that enabling CLKO does not interfere with the oscillator ready bit (SCLKRDY).
