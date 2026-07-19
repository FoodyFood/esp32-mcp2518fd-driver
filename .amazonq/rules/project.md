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

Use `tools/search.py` to query both documents:
  python tools/search.py <keyword1> <keyword2> ...
Results written to `docs/reference/search_results.txt`.

Never assume a register address or bit position. Always verify from the PDFs first.

## Development Workflow — Spec-Driven Closed Loop
Every feature must follow this exact sequence. Do not skip or reorder steps.
One feature per step. Do not combine multiple unverified features in one step.

### 1. Read the spec
- Open the relevant spec from `docs/specs/`
- Confirm all acceptance criteria are understood before writing any code
- Verify every register address, bit position and field definition against the PDFs
- **Record all findings** — any register layout detail, constraint, gotcha or non-obvious
  datasheet fact must be written into the spec under a `## Datasheet findings` section
  before implementation begins. This is the paper trail. Future work must not have to
  re-derive what was already discovered.

### 2. Design the solution
- Write out the planned changes to API, registers and RAM layout
- Identify all call sites that will be affected (examples, tests, existing API users)
- Confirm the design satisfies every acceptance criterion in the spec
- Do not write implementation code until the design is complete

### 3. Implement the solution
- Make the minimum code change that satisfies the spec — nothing more
- Keep each layer in its correct file (`registers.h`, `mcp2518fd_can.*`, `mcp2518fd_spi.*`)
- Update all affected call sites in examples and tests in the same change
- Build must pass before proceeding to testing
- **Do NOT use WSL or pio directly to build.** The integration test runner (verify.py) uploads
  and verifies on real hardware from Windows — it is the only build+test tool needed.
  Unit tests are the only exception (they run on the host via WSL, no hardware required).

### 4. Test on real hardware
Two ESP32 boards are available (COM4 and COM3).
All integration test commands run directly on Windows — never via WSL.

**single_node — always run first:**
```
python tests/integration/verify.py --suite single_node --port COM4
```

**id_filter — run for any spec touching filters or EID:**
```
python tests/integration/verify.py --suite id_filter --port COM4
```

**two_node — run for every spec touching TX, RX, filters, errors or timing:**
```
python tests/integration/verify.py --suite two_node --port COM4 --port-b COM3
```

**Additional hardware checks required by specific specs:**
- SPEC-003 (bus errors): bus disconnected, one node, MODE_NORMAL — verify NoAck and TEC increment
- SPEC-004 (interrupt RX): verify INT pin (GPIO 34) triggers under burst traffic from second node
- SPEC-005 (listen-only): Node A in MODE_LISTEN, Node B transmitting — verify Node B sees no errors
- SPEC-006 (stop/restart): verify stop() halts TX keepalives observed on second node's serial output

### 5. Commit
```
git add . && git commit -m "SPEC-NNN step N: short description"
```
- Only after all hardware assertions pass — no exceptions
- Update `docs/specs/README.md` status: Pending → In Progress → Done
- Update `docs/status.md` with observed hardware values
- Code and docs in the same commit. Never commit unverified code.

## Unit Tests

Unit tests live in `tests/unit/` as a self-contained PlatformIO project with a `native` env.
They run on the host via WSL — no hardware required.

```bash
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
```

**When to add unit tests:**
- Any pure-logic function with no hardware dependency is a unit test candidate.
- If a function can be extracted into `mcp2518fd_timing.h`, `mcp2518fd_presets.h`, or a similar
  hardware-free header, it must have unit tests before the spec is marked Done.
- New bit-packing logic (T0/R0 encode/decode, filter OBJ/MASK encoding) must have roundtrip tests.
- New register address helpers (inline constexpr address calculations) must have address value tests.
- New rejection/guard logic in calculation functions must have negative-case tests.
- Unit tests must pass (`50 succeeded` or equivalent) before committing any spec step that adds
  or modifies testable logic. Run them as part of the pre-commit check alongside the integration suite.
- Never remove or weaken an existing unit test assertion.

## CI (GitHub Actions)

Every PR runs `.github/workflows/ci.yml` which:
- Runs all 50 unit tests on `ubuntu-24.04` (native PlatformIO env, no hardware)
- Builds every example for ESP32 without uploading (catches compile errors on all examples)
- Auto-merges the PR via squash if all jobs pass

All examples use `lib_deps = file://../..` which works identically on Windows locally and on Linux in CI. No patching or symlinks required.

To add a new example to CI: add its directory name to the `matrix.example` list in `ci.yml`.

## Regression Testing
Run the full suite after every spec before marking it Done:
```
python tests/integration/verify.py --suite all --port COM4 --port-b COM3
```
All three suites must report PASS. A spec that passes single_node but not two_node is NOT verified.

**Tests are additive — never remove or weaken an existing assertion.** If a new feature
requires a configuration change that would break an existing test, add a new test block
after all existing ones rather than modifying them.

## Verification Standard
A spec is Done only when ALL of the following are true:
- Built and uploaded to real hardware on both nodes
- Every assertion in single_node and id_filter printed OK
- Every assertion in two_node printed OK on both COM4 and COM3
- Any spec-specific hardware check was performed and output matches acceptance criteria
- Observed register values match expected values derived from the datasheet

Only hardware output counts. Assumptions and reasoning are not verification.

## Example Validation — Run Before Any New Feature Work
Before starting a new spec, verify all existing examples in this order:
1. Automatable suites — `python tests/integration/verify.py --suite all --port COM4 --port-b COM3`
2. `examples/walkie_talkie` — manual interactive test, both nodes
3. `examples/scope_loopback` — single-board, COM4, observe on scope
4. `examples/bus_monitor` — both nodes, COM4 (node_a) + COM3 (node_b)

## Adding New Examples
- Create a new example when a feature does not fit cleanly into an existing one, or would
  make it too large or unfocused
- Each example is a self-contained PlatformIO project under `examples/<name>/`
- If automatable (deterministic pass/fail, no scope, no human interaction required) it
  **must** be added to the integration suite — add a suite to `tests/integration/mcp_test/suites.py`
  and register it in `tests/integration/mcp_test/runner.py`. Automatable examples that are
  not in the regression suite do not count as verified.
- If interactive or scope-based, document expected manual observation in the example's README
- Add the new example to the Files list below and the Examples table in `README.md`

## Key Implementation Rules
- NEVER do a 32-bit read-modify-write of CiCON — use byte-level write8() to CiCON+3 for REQOP
- NEVER read CiFIFOUAm while in Configuration mode — UA is only valid outside config mode
- NEVER enter config mode before validating that the requested rate is achievable — calcBitTiming() first, chip access second
- Always check TFNRFNIF before writing a TX message to RAM
- Always set UINC and TXREQ in the same write32() call when appending to a transmitting FIFO
- FRESET is set automatically in config mode and cleared on mode exit — do not poll it in config mode
- All registers are little-endian (LSB at lower address)
- FSYS = 20 MHz on this hardware — 8 Mbps data rate is not achievable (non-integer TQ count)

## API Design Principles
The public API is the primary product. Every design decision must be evaluated against:
- **Easy to consume** — the common case (send a frame, receive a frame) must be obvious and
  require minimal setup. A new user should be productive in under 10 lines.
- **Configurable enough** — advanced use cases (raw timing, custom oscillators, EID, filters)
  must be reachable without forking the driver or bypassing the API.
- **Single Responsibility** — each function, class and file has one reason to change.
  `CanMsg` owns message data. `MCP2518Driver` owns chip lifecycle and FIFO management.
  `MCP2518SPI` owns the wire protocol. Do not blur these boundaries.
- **Tidy first** — structural changes (rename, extract, reorganise) are committed separately
  from behavioural changes. Never mix a refactor with a feature in the same commit.
- **Additive, not breaking** — new fields on `CanMsg` must have safe zero-value defaults so
  existing callers compile and behave correctly without modification.
- **No hidden state** — every configuration decision visible to the caller must be
  expressible through the public API; nothing important should be buried in a private default.

## Code Style
- No third-party CAN libraries
- Minimal code — only what is needed to satisfy the current spec
- All register constants in `include/mcp2518fd_registers.h`
- All SPI transport in `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp`
- Public driver API in `include/mcp2518fd_can.h` / `src/mcp2518fd_can.cpp`
- Examples use only the public API — no register names, no raw addresses
- ISR functions must be marked `IRAM_ATTR` on ESP32
- No TEF, no TXQ — FIFO1=TX, FIFO2=RX only (filters route to FIFO2)

## Commit and Documentation
- Commit after every verified step — no exceptions
- Commit message format: `SPEC-NNN step N: short description`
- Code and docs always in the same commit
- After every verified step update:
  - `docs/status.md` — mark step complete with observed hardware values
  - `docs/registers.md` — if any new register fields were used or clarified
  - `docs/context.md` — if any new decisions, discoveries or gotchas were made
- If a register behaves unexpectedly, document it in `docs/context.md` immediately
- End every verified step with one plain-English sentence summarising what was achieved
  and how it moves closer to the goal

## Files
- `.github/workflows/ci.yml` — CI workflow: unit tests + build all examples on every PR, auto-merge on pass
- `examples/single_node/src/main.cpp` — single-board config and bitrate regression tests
- `examples/id_filter/src/main.cpp` — acceptance filter demonstration (SID/EID filtering)
- `examples/two_node/src/main.cpp` — two-node regression test (real bus, COM4 + COM3)
- `examples/walkie_talkie/` — interactive text chat between two nodes
- `examples/scope_loopback/` — continuous TX in MODE_EXTERNAL_LB for scope measurements
- `examples/bus_monitor/` — two nodes continuously transmitting counters (node_a → COM4, node_b → COM3)
- `include/mcp2518fd_can.h` — public driver API, CanMsg, CanStatus
- `include/mcp2518fd_presets.h` — bit timing preset constants (Arduino-free, used by unit tests)
- `include/mcp2518fd_timing.h` — pure-logic timing functions: calcBitTiming, calcTxTimeout, EID/filter encode (Arduino-free, used by unit tests)
- `include/mcp2518fd_registers.h` — all register addresses, masks, constants
- `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport + mode control
- `src/mcp2518fd_can.cpp` — driver implementation
- `tests/integration/verify.py` — integration test entry point (upload + verify, single suite or all)
- `tests/integration/mcp_test/` — runner, suites, upload, serial I/O modules
- `tests/unit/platformio.ini` — native PlatformIO env for host-side unit tests
- `tests/unit/test/test_unit/test_main.cpp` — 50 unit tests: dlcToLen, calcBitTiming, calcTxTimeout, EID encode/decode, filter encoding, register addresses, bit constants
- `tests/unit/README.md` — unit test coverage summary and run instructions
- `tools/search.py` — PDF search tool for datasheet verification
- `docs/status.md` — milestone tracker
- `docs/context.md` — hardware and architecture context
- `docs/registers.md` — register field reference
- `docs/use_case_coverage.md` — real-world use case coverage and gap analysis
- `docs/specs/README.md` — spec index and implementation status
- `docs/specs/SPEC-NNN-*.md` — individual feature specs
