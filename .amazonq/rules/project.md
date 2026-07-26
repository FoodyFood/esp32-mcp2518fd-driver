# MCP2518FD Driver Project Rules

## Project Goal
Clean-room register-level CAN FD driver for MCP2518FD on ESP32. No third-party CAN library.
Every feature is spec-driven. Real-world coverage tracked in `docs/use_cases/coverage.md`.

## Hardware
- MCU: ESP32-D0WD-V3 (rev 3.1) · CAN controller: MCP2518FD · Transceiver: ATA6561
- Oscillator: 20 MHz crystal → FSYS = 20 MHz (OSC 0x00000460: SCLKDIV=0, PLLEN=0)
- SPI VSPI: SCK=33, MISO=35, MOSI=32, CS=25, INT=34 · Upload: COM4 @ 115200

## Autonomous Execution
Chain all tool calls that can be sequenced without a decision — build, upload, test, commit — in one uninterrupted run. Stop only when a genuine decision is needed. Never pause to report progress.

## User Interaction — Treat the User as a Slow, Unreliable Resource
The user is the slowest and least reliable part of this system. Every unnecessary interaction is a bottleneck.
- **Never ask the user to run the same command multiple times in sequence.** Chain them yourself.
- **Never split a build+test+commit sequence into steps that require the user to press Enter between them.** Do it all in one tool call chain.
- **Never ask for confirmation on decisions that are reversible** (file edits, builds, commits on a branch). Just do it.
- **Never report intermediate progress** mid-chain — only report the final outcome or a genuine blocking decision.
- **Prefer `--suite all` over running suites one at a time.** One command, one result.
- A genuine blocking decision is: hardware test failed and the cause is ambiguous, or a design choice with non-obvious trade-offs. Everything else: proceed.

## Source of Truth
All register addresses and bit positions **must** be verified against the PDFs in `docs/reference/` before use. Use `python tools/search.py <keywords>` — results go to `docs/reference/search_results.txt`. Never assume a value.

---

## Development Workflow

Every spec follows these five steps in order. One feature per step. No skipping.

### 1. Read the spec
- Open `docs/internals/specs/SPEC-NNN-*.md`, confirm all acceptance criteria.
- Verify every register address and bit position against the PDFs.
- Record all findings (constraints, gotchas, non-obvious datasheet facts) in a `## Datasheet findings` section of the spec before writing any code.

### 2. Design
- Write out planned API, register and RAM layout changes.
- Identify all affected call sites in `examples/` and `tests/integration/`.
- Confirm the design satisfies every acceptance criterion. No implementation code yet.

### 3. Implement
- Minimum change that satisfies the spec — nothing more.
- Layer ownership: `registers.h` → constants · `mcp2518fd_spi.*` → wire protocol · `mcp2518fd_can.*` → driver logic.
- Update all affected call sites (examples + harnesses) in the same change.
- **Do NOT use WSL or pio directly to build.** Use `verify.py` for integration builds. Unit tests are the only WSL exception.
- **NEVER pipe or filter command output in any way.** No `findstr`, no `grep`, no `tail`, no `head`, no `| anything`. These hide errors and successes alike, cause silent failures, and always require a re-run. Always capture and display the full raw output directly.
- Build must pass before testing.

### 4. Test on real hardware
All integration commands run on **Windows** (never WSL). Two boards: COM4 and COM3.

**Always use `--suite all` in a single command.** Do not run suites one at a time and wait for the user between them.

```
python tests/integration/verify.py --suite all --port COM4 --port-b COM3
```

Only fall back to a single suite if `--suite all` fails and you need to isolate which suite is broken.

Spec-specific hardware checks:
- SPEC-003: bus disconnected, one node, MODE_NORMAL — verify NoAck + TEC increment
- SPEC-004: verify INT pin (GPIO 34) triggers under burst traffic from second node
- SPEC-005: Node A MODE_LISTEN, Node B TX — verify Node B sees no errors
- SPEC-006: verify stop() halts TX keepalives on second node's serial output

### 5. Commit
```
git add . && git commit -m "SPEC-NNN step N: short description"
```
- Only after all hardware assertions pass.
- Update `docs/internals/specs/README.md` status: Pending → In Progress → Done.
- Update `docs/status.md` with observed hardware values.
- Code and docs in the same commit. Never commit unverified code.

---

## Unit Tests

Run on host via WSL — no hardware required.
```bash
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && python3 coverage.py"
```

Coverage baseline: **100% lines, 100% functions, ~77% branches** (branch ceiling is a `constexpr` ternary property — not a gap).

**Add unit tests when:**
- Any pure-logic function extractable into `mcp2518fd_timing.h` or `mcp2518fd_presets.h`.
- New bit-packing logic (T0/R0, filter OBJ/MASK) → roundtrip tests.
- New register address helpers → address value tests.
- New guard/rejection logic → negative-case tests.
- New register bit constants → position/value tests.

Unit tests must pass and coverage must not regress before committing. Never remove or weaken an existing assertion.

**Design for testability:** Pure logic belongs in hardware-free headers. If you want to test something in `mcp2518fd_can.cpp`, extract it first. Hardware-dependent code is verified by integration tests only — no mocking.

---

## Examples vs Test Harnesses

These are distinct. Never mix them.

### `examples/` — user-facing only
- `platformio.ini` uses `lib_deps = foodyfood/esp32-mcp2518fd-driver` (published package, never a local path).
- First comment block must open with `// Learning objective:` and state in plain language what the user will be able to do after working through the example. This is the promise to the reader — make it concrete.
- Code flows in reading order: includes → pin constants → driver construction → setup → loop. No forward references, no helper functions defined after the call site that uses them.
- Comments explain *why*, not *what*. A reader who knows C++ does not need `// transmit the frame` above `can.transmit(tx)`. They do need to know *why* a particular mode, rate, or ID was chosen.
- **No SPEC-NNN references anywhere** — not in comments, not in variable names, not in Serial output. The user has never seen the spec. References to the development process are noise to them.
- **No register names, register addresses, or internal field names** (e.g. TBCPRE, FIFO2, TXREQ, CiCON). Use plain descriptions: "hardware timestamp", "receive buffer", "transmit request".
- **No internal driver terminology** that isn't part of the public API (e.g. "harness", "suite", "TEC", "REC", "FSYS" in explanatory prose — `getFsys()` in code is fine because it's the public API name).
- No CHECK() macros, no pass/fail output.
- Every example must have a `README.md` with: what you'll learn, hardware required, wiring table, setup steps (which binary goes where for two-board examples), and expected Serial output.
- Added **after** the feature is published to the PlatformIO registry — not during spec work.
- At spec close, explicitly assess whether a new example is warranted; note it as a follow-up if so.

### `tests/integration/<harness>/` — verification only
- `lib_deps = symlink://../../..` · CHECK() macros and SPEC-NNN references are correct here.
- Register in `tests/integration/mcp_test/runner.py` and add a `build-harnesses` matrix entry in `ci-checks.yml`.
- Create a new harness when a feature is large enough to warrant focused, isolated verification.

**Compatibility check on every feature:** both `examples/` and `tests/integration/` must compile and pass before committing. A change that breaks either is not done.

---

## CI (GitHub Actions)

`.github/workflows/ci-checks.yml` on every PR:
- Unit tests on `ubuntu-24.04` + coverage artefact upload.
- Build every example (pulls published package — catches API drift).
- Build every integration harness.

If a spec changes the public API, bump and publish the package before CI will pass for examples.
- New example → add to `matrix.example` in `ci-checks.yml`.
- New harness → add to `matrix.suite` in `ci-checks.yml`.

---

## Version Bumping

Tracked in `library.json` (`"version"`) and `library.properties` (`version=`) — always in sync.

First action on any new spec:
```
git checkout -b spec-NNN-short-description
git add library.json library.properties && git commit -m "SPEC-NNN step 0: bump version to 0.X.0"
```

All spec work stays on the branch until hardware verification is complete. PR to main only then. Never commit spec work directly to main.

| Version | Spec |
|---|---|
| 0.1.0 | Initial release |
| 0.2.0 | SPEC-002 (Filters) |
| 0.3.0 | SPEC-003 (Bus errors) |
| 0.4.0 | SPEC-004 (Interrupt RX + FIFO depth) |
| 0.5.0 | SPEC-005 (RX timestamp + listen-only) |
| 0.6.0 | SPEC-006 (Stop/restart/sleep) |
| 0.7.0 | SPEC-007 (API cleanup) |
| 0.8.0 | SPEC-008 (Classic CAN mode) |
| 0.9.0 | SPEC-009 (CLKO output) |
| 0.10.0 | SPEC-010 (Dual INT pins) |
| 0.11.0 | SPEC-011 (Battery simulator example) |

Update this table when a new spec is added.

---

## Documentation Gate

Before marking a spec Done, update in the **same commit as the code**:
- `docs/api.md` — any public type, method, parameter or return value added/changed
- `README.md` API tables — any public type or method added/changed
- `docs/status.md` — step complete with observed hardware values
- `docs/use_cases/coverage.md` — if the feature closes a gap or changes coverage status
- `docs/internals/context.md` — any new hardware discovery, gotcha or architectural decision

A spec that passes hardware but has stale docs is NOT Done. New public API without a `docs/api.md` entry will not be merged.

---

## Key Implementation Rules
- NEVER 32-bit read-modify-write CiCON — use `write8()` to CiCON+3 for REQOP.
- NEVER read CiFIFOUAm in Configuration mode — UA is only valid outside config mode.
- NEVER enter config mode before `calcBitTiming()` confirms the rate is achievable.
- NEVER write REG_OSC after `mSpi.setMode()` has been called — write it between `mSpi.reset()` and `mSpi.setMode(MODE_CONFIG)`. Writing OSC at any other point disables the RX path on this hardware (frames transmit OK but never arrive in FIFO2). Matches Microchip reference sequence: OscillatorControlSet is called immediately after Reset, before any other configuration.
- Always check TFNRFNIF before writing a TX message to RAM.
- Always set UINC and TXREQ in the same `write32()` call.
- FRESET is auto-set in config mode and cleared on exit — do not poll it in config mode.
- All registers are little-endian (LSB at lower address).
- FSYS = 20 MHz — 8 Mbps data rate is not achievable (non-integer TQ count).

## API Design Principles
- **Easy to consume** — common case (send/receive a frame) in under 10 lines.
- **Configurable enough** — advanced use cases (raw timing, EID, filters) reachable without forking.
- **Single Responsibility** — `CanMsg` owns message data · `MCP2518Driver` owns chip lifecycle · `MCP2518SPI` owns wire protocol. Do not blur these.
- **Tidy first** — structural changes committed separately from behavioural changes.
- **Additive, not breaking** — new `CanMsg` fields must have safe zero-value defaults.
- **No hidden state** — every configuration decision must be expressible through the public API.

## Code Style
- No third-party CAN libraries. Minimal code — only what the current spec requires.
- `include/mcp2518fd_registers.h` — all register constants
- `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport
- `include/mcp2518fd_can.h` / `src/mcp2518fd_can.cpp` — public driver API
- `include/mcp2518fd_timing.h` / `include/mcp2518fd_presets.h` — hardware-free logic
- Examples use only the public API — no register names, no raw addresses.
- ISR functions must be `IRAM_ATTR`.
- No TEF, no TXQ — FIFO1=TX, FIFO2=RX only.

---

## Files
- `include/mcp2518fd_can.h` — public API, CanMsg, CanStatus
- `include/mcp2518fd_registers.h` — register addresses, masks, constants
- `include/mcp2518fd_timing.h` — calcBitTiming, calcTxTimeout, EID/filter encode
- `include/mcp2518fd_presets.h` — bit timing preset constants
- `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport + mode control
- `src/mcp2518fd_can.cpp` — driver implementation
- `examples/walkie_talkie/` — send/receive variable-length messages, two nodes
- `examples/scope_loopback/` — continuous TX in MODE_EXTERNAL_LB for scope measurements
- `examples/int_pin/` — interrupt-driven RX via INT pin
- `tests/integration/verify.py` — integration test entry point
- `tests/integration/mcp_test/` — runner, suites, upload, serial I/O
- `tests/integration/single_node/src/main.cpp` — config, bitrates, error detection, FIFO, INT
- `tests/integration/id_filter/src/main.cpp` — SID/EID exact, range, multi-filter, catch-all
- `tests/integration/two_node/src/main.cpp` — real bus, COM4 + COM3
- `tests/unit/platformio.ini` — native env for host-side unit tests
- `tests/unit/coverage.py` — lcov + genhtml coverage report
- `tests/unit/test/test_unit/test_main.cpp` — 88 unit tests
- `tools/search.py` — PDF search tool
- `docs/api.md` — public API reference
- `docs/hardware.md` — pin table, SPI config, bus wiring
- `docs/status.md` — milestone tracker
- `docs/internals/context.md` — hardware and architecture context
- `docs/internals/registers.md` — register field reference
- `docs/use_cases/coverage.md` — use case coverage matrix
- `docs/use_cases/uc-dala-battery-emulator.md` — Battery-Emulator integration requirements
- `docs/internals/specs/README.md` — spec index and status
- `docs/internals/specs/SPEC-NNN-*.md` — individual feature specs
- `.github/workflows/ci-checks.yml` — CI: unit tests + build examples + build harnesses
- `CONTRIBUTING.md` — contribution guide
