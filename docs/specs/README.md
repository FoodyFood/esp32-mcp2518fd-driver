# Spec Index

Each spec closes one or more gaps from [`docs/use_cases/coverage.md`](../use_cases/coverage.md).
Specs are ordered by priority and grouped by theme.
Implement in order — later specs may depend on earlier ones (noted below).

## How to work a spec

1. **Read** — open the spec, understand every acceptance criterion, verify all register references against the PDFs
2. **Design** — plan the API and register changes; confirm the design satisfies every criterion before writing code
3. **Implement** — minimum code to satisfy the spec; update all affected call sites and examples in the same change
4. **Test on hardware** — loopback first (single board, COM4), then two-node (COM4 + COM3); both must pass all assertions; run any spec-specific hardware check listed in the spec
5. **Commit** — only after hardware passes; update Status below from `Pending` → `In Progress` → `Done`

Never mark a spec Done without two-node hardware evidence.

---

## Group A — EV Battery (highest priority)

These two specs together unlock UC-1 (EV Battery Gateway) and UC-3 (UDS Diagnostics)
for batteries that use 29-bit IDs (VW MEB, Rivian, Ford Mach-E).
SPEC-002 depends on SPEC-001 for the EID filter path.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-001](SPEC-001-extended-id.md) | 29-bit Extended ID (EID) Support | G1 | Done |
| [SPEC-002](SPEC-002-acceptance-filters.md) | Acceptance Filter API | G2 | Done |

## Group B — Error Visibility (EV battery + inverter)

Shared diagnostic surface needed by both the BMS gateway and the inverter interface.
No dependencies on Group A — can be implemented in parallel.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-003](SPEC-003-bus-error-and-tx-result.md) | Bus Error Detection and TX Error Detail | G3, G9 | Done |

## Group C — Frame Reliability (inverter + logger)

Protects against frame loss on high-traffic buses. SPEC-004 references `getErrors()`
from SPEC-003 for the overflow acceptance criterion — implement SPEC-003 first.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-004](SPEC-004-interrupt-rx-and-fifo-depth.md) | Interrupt-driven RX and Configurable FIFO Depth | G4, G8 | Done |

## Group D — Passive Monitoring (logger + BMS tooling)

Timestamping and listen-only mode serve the data logger and any passive monitoring tool.
No hard dependencies, but SPEC-003's `CanTxResult` is referenced in the listen-only
transmit behaviour — implement SPEC-003 first.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-005](SPEC-005-rx-timestamp-and-listen-only.md) | RX Timestamp and Listen-Only Mode Validation | G6, G10 | Done |

## Group E — Lifecycle (BMS reset sequences)

Stop/restart and sleep/wake. Self-contained, no dependencies.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-006](SPEC-006-stop-restart-sleep.md) | Stop, Restart and Sleep/Wake Lifecycle | G7 | Done |

## Group F — API Cleanup (do before new features)

Resolves structural API issues identified in the pre-SPEC-008 audit.
No hardware changes. Must be done before SPEC-008 to avoid compounding the issues.

| Spec | Title | Status |
|---|---|---|
| [SPEC-007](SPEC-007-api-review.md) | Public API Review and Cleanup | Pending |

## Group G — Battery-Emulator Integration

These three specs close the gaps identified in
[`docs/use_cases/uc-dala-battery-emulator.md`](../use_cases/uc-dala-battery-emulator.md)
that block a clean drop-in replacement of ACAN2517FD. Implement in order — SPEC-009
(CLKO) must come before SPEC-010 (dual INT) on dual-chip boards where the second chip
is clocked from the first.

| Spec | Title | IR closed | Status |
|---|---|---|---|
| [SPEC-008](SPEC-008-classic-can-mode.md) | Classic CAN Mode on FD Chip (Normal20B) | IR-18 | Pending |
| [SPEC-009](SPEC-009-clko-output.md) | CLKO Output Pin Configuration | IR-19 | Pending |
| [SPEC-010](SPEC-010-dual-int-pins.md) | Dual INT Pin Support (INT0 / INT1) | IR-20 | Pending |

## Group G — Real-World Examples (consumes previous specs)

End-to-end examples that exercise the full driver API against real-world protocols.
Each spec in this group depends on the feature specs listed against it being Done first.

| Spec | Title | Depends on | Status |
|---|---|---|---|
| [SPEC-011](SPEC-011-battery-simulator-example.md) | CAN FD Battery Simulator Example (Kia 64 FD + VW MEB) | SPEC-001, SPEC-002, SPEC-003 | Pending |

---

## Coverage map

| Gap | Description | Spec |
|---|---|---|
| G1 | 29-bit extended ID | SPEC-001 |
| G2 | Acceptance filters | SPEC-002 |
| G3 | Bus error / bus-off detection | SPEC-003 |
| G4 | Interrupt-driven RX | SPEC-004 |
| G5 | RX overflow detection | SPEC-003 |
| G6 | Per-frame RX timestamp | SPEC-005 |
| G7 | stop() / restart() | SPEC-006 |
| G8 | Configurable RX FIFO depth | SPEC-004 |
| G9 | TX error distinction | SPEC-003 |
| G10 | Listen-only mode validation | SPEC-005 |
| IR-18 | Classic CAN mode on FD chip | SPEC-008 |
| IR-19 | CLKO output configuration | SPEC-009 |
| IR-20 | Dual INT pin support | SPEC-010 |
