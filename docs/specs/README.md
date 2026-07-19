# Spec Index

Each spec closes one or more gaps from [`docs/use_case_coverage.md`](../use_case_coverage.md).
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
| [SPEC-003](SPEC-003-bus-error-and-tx-result.md) | Bus Error Detection and TX Error Detail | G3, G9 | Pending |

## Group C — Frame Reliability (inverter + logger)

Protects against frame loss on high-traffic buses. SPEC-004 references `getErrors()`
from SPEC-003 for the overflow acceptance criterion — implement SPEC-003 first.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-004](SPEC-004-interrupt-rx-and-fifo-depth.md) | Interrupt-driven RX and Configurable FIFO Depth | G4, G8 | Pending |

## Group D — Passive Monitoring (logger + BMS tooling)

Timestamping and listen-only mode serve the data logger and any passive monitoring tool.
No hard dependencies, but SPEC-003's `CanTxResult` is referenced in the listen-only
transmit behaviour — implement SPEC-003 first.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-005](SPEC-005-rx-timestamp-and-listen-only.md) | RX Timestamp and Listen-Only Mode Validation | G6, G10 | Pending |

## Group E — Lifecycle (BMS reset sequences)

Stop/restart and sleep/wake. Self-contained, no dependencies.

| Spec | Title | Gaps closed | Status |
|---|---|---|---|
| [SPEC-006](SPEC-006-stop-restart-sleep.md) | Stop, Restart and Sleep/Wake Lifecycle | G7 | Pending |

## Group F — Real-World Examples (consumes previous specs)

End-to-end examples that exercise the full driver API against real-world protocols.
Each spec in this group depends on the feature specs listed against it being Done first.

| Spec | Title | Depends on | Status |
|---|---|---|---|
| [SPEC-007](SPEC-007-battery-simulator-example.md) | CAN FD Battery Simulator Example (Kia 64 FD + VW MEB) | SPEC-001, SPEC-002, SPEC-003 | Pending |

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
