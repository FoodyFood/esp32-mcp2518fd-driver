# MCP2518FD Driver Project Rules

## Project Goal
Clean-room direct register-level CAN FD driver for MCP2518FD on ESP32.
No third-party CAN library. Target: full real-world CAN FD coverage across EV battery,
inverter, logger and diagnostic use cases — tracked in `docs/use_case_coverage.md`.
Every feature is driven by a spec in `docs/specs/` before any code is written.

## Hardware
- MCU: ESP32-D0WD-V3 (rev 3.1)
- CAN controller: MCP2518FD
- Oscillator: 20 MHz crystal (OSC reg 0x00000460: SCLKDIV=0, PLLEN=0 → FSYS=20 MHz)
- Transceiver: ATA6561
- SPI: VSPI — SCK=33, MISO=35, MOSI=32, CS=25, INT=34
- Upload port: COM4 @ 115200 baud

## Autonomous Execution
Minimise the number of times the user must intervene. Chain all tool calls that can be
sequenced without a decision point — build, upload, test, commit — in a single uninterrupted
run. Only stop and ask when a genuine decision is required (e.g. a test fails and the fix is
not obvious, or a spec is ambiguous). Never pause between steps just to report progress.
The user is the slowest part of the system; keep them out of the loop until their input
actually changes the outcome.

## Source of Truth
All register addresses, bit positions and field definitions MUST be verified against:
- `docs/reference/External-CAN-FD-Controller-with-SPI-Interface-DS20006027B.pdf` (MCP2518FD datasheet)
- `docs/reference/MCP25XXFD-CAN-FD-Controller-Module-Family-Reference-Manual-DS20005678E.pdf` (family reference manual)

Use `docs/search.py` to query both documents:
  python docs/search.py <keyword1> <keyword2> ...
Results written to `docs/reference/search_results.txt`.

Never assume a register address or bit position. Always verify from the PDFs first.

## Example Validation — Run Before Any New Feature Work
Before starting any new spec or feature, all existing examples must be verified on hardware.
Verify examples one at a time in this order:
1. `examples/loopback` — single-board, COM4 only
2. `examples/two_node` — both nodes, COM4 + COM3
3. `examples/walkie_talkie` — manual interactive test, both nodes
4. `examples/scope_loopback` — single-board, COM4, observe on scope
5. `examples/bus_monitor` — both nodes, COM4 + COM3

Each example must build cleanly and produce expected output before moving to the next.
Do not proceed to spec-driven feature work until all examples are verified.

## Adding New Examples
- Create a new example when a new feature does not fit cleanly into `loopback` or `two_node`,
  or when adding it would make an existing example too large or unfocused
- Each example is a self-contained PlatformIO project under `examples/<name>/`
- If the example is automatable (deterministic pass/fail output), add support to `tools/run_test.py`
- If the example is interactive or scope-based, document the expected manual observation in the example's README
- Add the new example to the Files list and Examples table in this file and in `README.md`

## Development Workflow — Spec-Driven Closed Loop
Every feature must follow this exact sequence. Do not skip or reorder steps.

### 1. Read the spec
- Open the relevant spec from `docs/specs/` (e.g. `SPEC-001-extended-id.md`)
- Confirm all acceptance criteria are understood before writing any code
- If the spec is ambiguous, resolve it by querying the datasheets first — never assume
- Verify every register address, bit position and field definition cited in the spec
  against the PDFs before touching any code

### 2. Design the solution
- Write out the planned changes to API, registers and RAM layout as comments or notes
- Identify all call sites that will be affected (examples, tests, existing API users)
- Confirm the design satisfies every acceptance criterion in the spec
- Do not write implementation code until the design is complete

### 3. Implement the solution
- Make the minimum code change that satisfies the spec — nothing more
- Update `include/mcp2518fd_can.h`, `src/mcp2518fd_can.cpp`, `include/mcp2518fd_registers.h`
  as needed; keep each layer in its correct file
- Update all affected call sites in examples and tests in the same commit
- Build must pass before proceeding to testing

### 4. Test on real hardware — both nodes
Two ESP32 boards are available (COM4 and COM3). Use both for every spec.

**Single-board (loopback) — always run first:**
```
cd examples/loopback
pio run -e loopback --target upload --upload-port COM4
python ../../tools/run_test.py --env loopback --port COM4
```
All assertions must print OK before proceeding to two-node.

**Two-node (real bus) — run for every spec that touches TX, RX, filters, errors or timing:**
```
cd examples/two_node
pio run -e two_node --target upload --upload-port COM4
pio run -e two_node --target upload --upload-port COM3
python ../../tools/run_test.py --env two_node --port-a COM4 --port-b COM3
```
Both nodes must report all assertions OK.

**After every spec, re-run loopback and two_node in full to confirm no regressions.**
If a new feature has a dedicated example, run that example too before committing.

**Additional hardware checks required by specific specs:**
- SPEC-003 (bus errors): test with bus disconnected — one node only, MODE_NORMAL, verify NoAck and TEC increment
- SPEC-004 (interrupt RX): verify INT pin (GPIO 34) triggers correctly under burst traffic from second node
- SPEC-005 (listen-only): Node A in MODE_LISTEN, Node B transmitting — verify Node B sees no errors
- SPEC-006 (stop/restart): verify stop() halts TX keepalives observed on second node's serial output

### 5. Commit
```
git add . && git commit -m "SPEC-NNN step N: short description"
```
- Only after all hardware assertions pass — no exceptions
- Update `docs/specs/README.md` spec Status from Pending → In Progress → Done
- Update `docs/status.md` with observed hardware values
- Docs and code in the same commit

Never commit unverified code. Never mark a spec Done without two-node hardware evidence.

## Verification Standard
A spec is only considered verified when ALL of the following are true:
- The code was built and uploaded to real hardware on both nodes
- Every assertion in the loopback test printed "OK" — no FAILs, no skipped checks
- Every assertion in the two-node test printed "OK" on both COM4 and COM3
- Any spec-specific hardware check (bus disconnect, interrupt pin, listen-only) was performed
  and the observed output matches the acceptance criteria in the spec
- The observed register values match the expected values derived from the datasheet

Assumptions, reasoning, or "it should work" are NOT verification. Only hardware output counts.
A spec that passes loopback but has not been tested two-node is NOT verified.

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
- Minimal code — only what is needed to satisfy the current spec
- All register constants in `include/mcp2518fd_registers.h`
- All SPI transport in `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp`
- Public driver API in `include/mcp2518fd_can.h` / `src/mcp2518fd_can.cpp`
- Examples use only the public API — no register names, no raw addresses
- Test harness in `examples/loopback/src/main.cpp` — key-triggered via Serial
- ISR functions must be marked `IRAM_ATTR` on ESP32
- No TEF, no TXQ — FIFO1=TX, FIFO2=RX only (filters route to FIFO2)

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
- `examples/loopback/src/main.cpp` — regression test harness (single-board)
- `examples/two_node/src/main.cpp` — two-node regression test (real bus, COM4 + COM3)
- `examples/walkie_talkie/` — interactive text chat between two nodes
- `examples/scope_loopback/` — continuous TX in MODE_EXTERNAL_LB for scope measurements
- `examples/bus_monitor/` — two nodes continuously transmitting counters
- `include/mcp2518fd_can.h` — public driver API, CanMsg, CanStatus, bit timing presets
- `include/mcp2518fd_registers.h` — all register addresses, masks, constants
- `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport + mode control
- `src/mcp2518fd_can.cpp` — driver implementation
- `tools/run_test.py` — test runner (loopback and two-node)
- `tools/check_timing.py` — verify bit timing presets for both 20 MHz and 40 MHz
- `docs/status.md` — milestone tracker
- `docs/context.md` — hardware and architecture context
- `docs/registers.md` — register field reference
- `docs/use_case_coverage.md` — real-world use case coverage and gap analysis
- `docs/specs/README.md` — spec index and implementation status
- `docs/specs/SPEC-NNN-*.md` — individual feature specs
- `docs/search.py` — PDF search tool
