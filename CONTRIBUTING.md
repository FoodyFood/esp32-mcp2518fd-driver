# Contributing

Contributions are welcome — bug reports, hardware compatibility reports, new examples, and feature PRs.

## What this driver is

A direct register-level CAN FD driver for the MCP2518FD on ESP32. No third-party CAN library. Every register write traces to a datasheet page. Every feature is verified on real hardware before it ships.

If you're adding a feature, that standard applies to your contribution too.

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) 6.x
- Espressif32 platform 7.0.1
- Python 3 (for the integration test runner)
- Two ESP32 boards wired to MCP2518FD modules for integration testing (one board is enough for single-node tests)

## Build and unit test

Unit tests run on the host — no hardware required:

```bash
cd tests/unit
pio test -e native
```

All 50+ tests must pass before opening a PR.

## Integration tests

Integration tests run on real hardware. Two boards are needed for the two-node suite.

```bash
# Single board (loopback, filters, error detection, INT pin)
python tests/integration/verify.py --suite single_node --port <PORT>

# Two boards (real bus, bidirectional)
python tests/integration/verify.py --suite two_node --port <PORT_A> --port-b <PORT_B>

# Full regression
python tests/integration/verify.py --suite all --port <PORT_A> --port-b <PORT_B>
```

All three suites must report PASS before a feature PR is accepted.

## Adding a feature

Features follow a spec-driven workflow documented in `docs/internals/specs/README.md`. The short version:

1. Open an issue describing the feature and the use case it serves.
2. Write a spec in `docs/internals/specs/` with explicit acceptance criteria.
3. Implement the minimum code that satisfies the spec.
4. Verify on real hardware — both single-node and two-node suites must pass.
5. Update `docs/api.md` if any public type or method changed.
6. Update the README API tables if any public type or method changed.
7. Open a PR with hardware evidence in the description.

PRs without hardware evidence will not be merged.

## Code style

- Register constants belong in `include/mcp2518fd_registers.h`
- SPI transport belongs in `include/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp`
- Public API belongs in `include/mcp2518fd_can.h` / `src/mcp2518fd_can.cpp`
- Pure logic with no hardware dependency belongs in `include/mcp2518fd_timing.h` and must have unit tests
- Examples use only the public API — no register names, no raw addresses
- Minimal code — only what is needed to satisfy the spec

## Reporting a bug

Open an issue with:
- The driver version (from `library.json`)
- Your hardware (MCU, CAN controller, oscillator frequency, transceiver)
- The `configure()` call and mode you used
- What you expected vs what happened
- Serial output if available

## Questions

Open an issue or start a discussion on GitHub.
