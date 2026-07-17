# MCP2518FD Driver Project Rules

## Project Goal
Clean-room direct register-level CAN FD driver for MCP2518FD on ESP32.
No third-party CAN library. Target: two-node CAN FD communication via ATA6561 transceiver.

## Hardware
- MCU: ESP32-D0WD-V3 (rev 3.1), 40MHz crystal
- CAN controller: MCP2518FD
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
2. Build: `pio run`
3. Upload: `pio run --target upload --upload-port COM4`
4. Read hardware output: `python monitor.py COM4 115200`
5. Verify the output matches expected values from the datasheet
6. If verification passes, commit: `git add . && git commit -m "step N: description"`
7. If verification fails, diagnose before proceeding

Never commit unverified code.

## Regression Testing
Every runTest() must include ALL previously verified checks, not just the new ones.
Before adding a new test, confirm all existing assertions still pass.
If any previously passing check fails, stop and fix the regression before continuing.

## Step-by-Step Discipline
- One feature per step
- Each step has a clear expected output defined before writing code
- Steps are numbered and tracked in `docs/status.md`
- Do not combine multiple unverified features in one step

## Key Implementation Rules
- NEVER do a 32-bit read-modify-write of CiCON — use byte-level write8() to CiCON+3 for REQOP
- NEVER read CiFIFOUAm while in Configuration mode — UA is only valid outside config mode
- Always check TFNRFNIF before writing a TX message to RAM
- Always set UINC and TXREQ in the same write32() call when appending to a transmitting FIFO
- FRESET is set automatically in config mode and cleared automatically on mode exit — do not poll it in config mode
- All registers are little-endian (LSB at lower address)

## Code Style
- No third-party CAN libraries
- No interrupts (polling only for now)
- No TEF, no filters, no TXQ (use FIFO1=TX, FIFO2=RX only)
- Minimal code — only what is needed for the current milestone
- All register constants in `mcp2518fd_registers.h`
- All SPI transport in `mcp2518fd_spi.h` / `mcp2518fd_spi.cpp`
- Test harness in `main.cpp` — key-triggered via Serial

## Files
- `src/main.cpp` — interactive test harness
- `src/mcp2518fd_registers.h` — all register addresses, masks, constants
- `src/mcp2518fd_spi.h` / `src/mcp2518fd_spi.cpp` — SPI transport + mode control
- `monitor.py` — closed-loop serial monitor (reset, trigger, read, exit)
- `docs/status.md` — milestone tracker
- `docs/context.md` — hardware and architecture context
- `docs/registers.md` — register field reference
- `docs/search.py` — PDF search tool
