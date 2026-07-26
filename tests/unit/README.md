# Unit Tests

Native unit tests — no hardware required.

## Run tests

```bash
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
```

## Run coverage report

Run tests first (produces `.gcda` files), then:

```bash
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && python3 coverage.py"
```

HTML report is written to `tests/unit/coverage/html/index.html`.

## Coverage baseline

| Metric | Rate |
|---|---|
| Lines | 100% (65/65) |
| Functions | 100% (14/14) |
| Branches | 77.1% (37/48) |

Branch coverage is below 100% because gcc instruments every arm of the `constexpr`
ternary chains in `dlcToLen` and `calcBitTiming`. Each call exercises one path through
the chain — the untaken arms are not independently reachable. 67–68% is the practical
ceiling for this code shape.

## Coverage scope

Only the driver's own header logic is measured — Unity and unity_config are excluded.
The files under test are:
- `include/mcp2518fd_registers.h` — register addresses, bit constants, address helpers
- `include/mcp2518fd_timing.h` — `calcBitTiming`, `calcTxTimeout`, EID encode/decode, filter encoding

`mcp2518fd_can.cpp` and `mcp2518fd_spi.cpp` are hardware-dependent and are not included
in the native unit test build. Their correctness is verified by the integration test suites.

## Test coverage summary

| Area | Tests |
|---|---|
| `dlcToLen()` | DLC 0–8 (1:1), DLC 9–15 (FD jumps) |
| `calcBitTiming()` | All nominal rates (125K/250K/500K/1M) at 20 MHz and 40 MHz vs presets; all data rates (1M/2M/4M/5M) at 20 MHz and 40 MHz vs presets; 8 Mbps at 40 MHz; TDC enabled/disabled/TDCO for all rates at both frequencies; BRP always zero; nominal and data SJW == TSEG2; nominal and data sample point 75–85%; rejection of non-integer TQ, zero inputs, totalTQ < 3, rate too high |
| `calcTxTimeout()` | Minimum floor of 2 ms; fsys=0 fallback; reasonable range at 20 MHz and 40 MHz; slower data rate → longer timeout; slower nominal rate → longer timeout |
| EID encode/decode | Roundtrip max/zero/known IDs; SID in bits[10:0]; EID in bits[28:11] |
| Filter OBJ/MASK | EXIDE bit set for EID OBJ; MIDE bit set for EID MASK; zero mask = don't-care; roundtrip known UDS ID; full 29-bit mask roundtrip |
| Register addresses | FIFO_CON/STA/UA for FIFO1 and FIFO2; 0x0C stride; FLTOBJ/FLTMSK 8-byte stride; FLTMSK offset +4 from FLTOBJ; FLTCON_REG groups of 4; FLTCON_BYTE wraps at 4; filter 0 and filter 31 addresses; key SFR addresses (CiCON, CiNBTCFG, CiDBTCFG, CiTDC, CiTSCON, CiINT, CiRXOVIF, CiTREC, OSC); RAM_BASE |
| FIFOCON/FIFOSTA bits | TXEN bit7, UINC bit8, TXREQ bit9, TFNRFNIF bit0, TXERR bit5, TXABT bit7, RXOVIF bit3, TXLARB bit6, TXATIF bit4 |
| CiTREC bits | EWARN bit16, RXWARN bit17, TXWARN bit18, RXBP bit19, TXBP bit20, TXBO bit21 |
| Other register constants | TSCON_TBCEN bit16, OSC_OSCDIS bit2, RXOVIF_FIFO2 bit2, CON2_RTXAT bit0, CINT2_RXIE bit1 |
| Mode constants | MODE_NORMAL=0 through MODE_RESTRICTED=7 |
