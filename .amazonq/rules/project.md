# MCP2518FD Driver Project Rules

## Project Goal
Clean-room direct register-level CAN FD driver for MCP2518FD on ESP32.
No third-party CAN library. Target: two-node CAN FD communication via ATA6561 transceiver.

## Hardware
- MCU: ESP32-D0WD-V3 (rev 3.1)
- CAN controller: MCP2518FD
- Oscillator: 20 MHz crystal (OSC reg 0x00000460: SCLKDIV=0, PLLEN=0 → FSYS=20 MHz)
- Transceiver: ATA6561
- SPI: VSPI — SCK=33, MISO=35, MOSI=32, CS=25, INT=34
- Upload port: COM4 @ 115200 baud

## Source of Truth
All register addresses, bit positions and field definitions MUST be verified against:
- `docs/reference/External-CAN-FD-Controller-with-SPI-Interface-DS20006027B.pdf` (MCP2518FD datasheet)
- `docs/reference/MCP25XXFD-CAN-FD-Controller-Module-Family-Reference-Manual-DS20005678E.pdf` (family reference manual)

Use `docs/search.py` to query both documents:
  python docs/search.py <keyword1> <keyword2> ...
Results written to `docs/reference/search_results.txt`.

Never assume a register address or bit position. Always verify from the PDFs first.

## Development Workflow — Closed Loop
Every change must follow this exact sequence. Do not skip steps.

1. Make the code change
2. Build + Upload + Test: `cd examples/loopback && pio run -e loopback --target upload --upload-port COM4 && python ../../tools/run_test.py --env loopback --port COM4`
3. Verify all assertions print OK — no FAILs
4. If verification passes, commit: `git add . && git commit -m "step N: description"`
5. If verification fails, diagnose before proceeding

Never commit unverified code.

## Verification Standard
A step is only considered verified when ALL of the following are true:
- The code was built and uploaded to real hardware
- The serial output was read back via monitor.py
- Every assertion in runTest() printed "OK" — no FAILs, no skipped checks
- The observed values match the expected values derived from the datasheet

Assumptions, reasoning, or "it should work" are NOT verification. Only hardware output counts.

## Regression Testing
Every runTest() must include ALL previously verified checks, not just the new ones.
Before adding a new test, confirm all existing assertions still pass.
If any previously passing check fails, stop and fix the regression before continuing.

- Tests are ADDITIVE — never remove or replace an existing assertion
- If a new feature requires a configuration change that would break an existing test, add a NEW separate test block after all existing ones rather than modifying the existing ones
- Existing test blocks must remain byte-for-byte identical to their last verified state
- If an existing spot-check is found to be incomplete (e.g. only checking first/last byte), expand it in-place — this is a fix, not a replacement

## Step-by-Step Discipline
- One feature per step
- Each step has a clear expected output defined before writing code
- Steps are numbered and tracked in `docs/status.md`
- Do not combine multiple unverified features in one step

## Key Implementation Rules
- NEVER do a 32-bit read-modify-write of CiCON — use byte-level write8() to CiCON+3 for REQOP
- NEVER read CiFIFOUAm while in Configuration mode — UA is only valid outside config mode
- NEVER enter config mode before validating that the requested rate is achievable — calcBitTiming() first, chip access second
- Always check TFNRFNIF before writing a TX message to RAM
- Always set UINC and TXREQ in the same write32() call when appending to a transmitting FIFO
- FRESET is set automatically in config mode and cleared automatically on mode exit — do not poll it in config mode
- All registers are little-endian (LSB at lower address)
- FSYS = 20 MHz on this hardware — 8 Mbps data rate is not achievable (non-integer TQ count)

## Code Style
- No third-party CAN libraries
- No interrupts (polling only for now)
- No TEF, no filters, no TXQ (use FIFO1=TX, FIFO2=RX only)
- Minimal code — only what is needed for the current milestone
- All register constants in `include/mcp2518fd_registers.h`
- All SPI transport in `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp`
- Public driver API in `include/mcp2518fd_can.h` / `src/mcp2518fd_can.cpp`
- Examples use only the public API — no register names, no raw addresses
- Test harness in `examples/loopback/src/main.cpp` — key-triggered via Serial

## Documentation
After every successful verified step:
- Update `docs/status.md` to mark the step complete with notes on actual observed values
- Update `docs/registers.md` if any new register fields were used or clarified
- Update `docs/context.md` if any new decisions, discoveries or gotchas were made
- Commit everything together: code + docs in the same commit

If something unexpected is discovered during testing (e.g. a register behaves differently than the datasheet suggests), document it immediately in `docs/context.md` under a "Discoveries" section before moving on.

After every verified step, end with a single plain-English sentence summarising what was achieved and specifically how it moves closer to the goal of loopback/CAN FD communication.

## Commit Rules
- Commit after every successful verified iteration — no exceptions
- Commit message format: `step N: short description`
- Never commit code that has not been verified on hardware
- Never commit with failing or skipped checks
- Docs and code go in the same commit

## Files
- `examples/loopback/src/main.cpp` — regression test harness
- `include/mcp2518fd_can.h` — public driver API, CanMsg, CanStatus, bit timing presets
- `include/mcp2518fd_registers.h` — all register addresses, masks, constants
- `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport + mode control
- `src/mcp2518fd_can.cpp` — driver implementation
- `tools/run_test.py` — test runner (loopback and two-node)
- `tools/check_timing.py` — verify bit timing presets for both 20 MHz and 40 MHz
- `docs/status.md` — milestone tracker
- `docs/context.md` — hardware and architecture context
- `docs/registers.md` — register field reference
- `docs/search.py` — PDF search tool
