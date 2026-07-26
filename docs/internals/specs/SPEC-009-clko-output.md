# SPEC-009 — CLKO Output Pin Configuration

## Status
Done

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

**OSC register 0xE00, byte 0 (bits 7:0) — DS20006027B Register 3-1, page 16:**
- `CLKODIV[1:0]` at bits 6:5. Encoding: `00`=÷1, `01`=÷2, `10`=÷4, `11`=÷10.
- Reset default: both bits are `R/W-1` → reset value = `0b11` = ÷10. CLKO is active at reset.
- **There is no CLKOEN bit.** The CLKO pin is always driven. "Disabled" in the spec means the
  caller does not connect the pin; the driver leaves CLKODIV at reset default (÷10).
- CLKODIV has no config-mode restriction — it is R/W at any time.
- IOCON.SOF (bit 29 of IOCON at 0xE04) repurposes the CLKO/SOF pin to output a SOF pulse
  instead of a clock. This is orthogonal to CLKODIV and not touched by this spec.
- OSCREADY (OSC bit 10) is unaffected by CLKODIV — no interference with clock stability detection.

**Implication for AC-3:** "CLKO disabled by default" means the driver does not write CLKODIV
when `clkoDivider=0`, leaving the reset default (÷10) in place. The pin is not intentionally driven
at a useful frequency, so existing callers are unaffected in practice.

**Implication for AC-4:** 40 MHz oscillator with `clkoDivider=10` → CLKODIV=`11` → CLKO = 4 MHz.
Second chip with 4 MHz input and PLLEN=1 → FSYS = 40 MHz. This is the T-2CAN FD use case.

**API correction:** The spec draft says `clkoDivider=10` maps to CLKODIV=`11`. The accepted
divisor values are 1, 2, 4, 10 only. Any other value is rejected with RATE_NOT_ACHIEVABLE.

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
// CanConfig gains a clkoDivider field:
// 0 = leave at reset default (CLKO pin not intentionally used)
// 1, 2, 4, 10 = set CLKODIV to divide SYSCLK by that value
struct CanConfig {
    uint8_t rxFifoDepth     = 16;
    bool    enableTimestamp = false;
    uint8_t clkoDivider     = 0;   // 0=default, 1/2/4/10=active
};

// configureRaw() also accepts CanConfig, so clkoDivider is reachable there too.
// Invalid clkoDivider values (not in {0,1,2,4,10}) return RATE_NOT_ACHIEVABLE.
```

## Notes
- The CLKODIV encoding in the OSC register must be verified from the datasheet before
  any code is written. Do not assume it is a linear divider value.
- `configureRaw()` should also accept a `clkoDivider` parameter for consistency.
- The autodetect path in `configure()` reads the OSC register to determine FSYS. Confirm
  that enabling CLKO does not interfere with the oscillator ready bit (SCLKRDY).
