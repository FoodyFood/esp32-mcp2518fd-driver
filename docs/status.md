# Verified Status

Each item below has been tested on real hardware and confirmed working.

## Unit Tests

| Area | Count | Status |
|---|---|---|
| `dlcToLen()` | 2 | ✅ Passing |
| `calcBitTiming()` — rate vs presets (20/40 MHz) | 8 | ✅ Passing |
| `calcBitTiming()` — TDC enabled/disabled/TDCO | 4 | ✅ Passing |
| `calcBitTiming()` — rejection cases | 5 | ✅ Passing |
| `calcBitTiming()` — sample point / SJW | 2 | ✅ Passing |
| `calcTxTimeout()` | 3 | ✅ Passing |
| EID encode/decode roundtrip | 5 | ✅ Passing |
| Filter OBJ/MASK encoding | 5 | ✅ Passing |
| Register address helpers | 10 | ✅ Passing |
| FIFOCON/FIFOSTA bit constants | 6 | ✅ Passing |
| **Total** | **50** | **✅ All passing** |

Run: `wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"`

## CI

| Check | Status |
|---|---|
| Unit tests (native, ubuntu-24.04) | ✅ Configured |
| Build all 6 examples (ESP32, no upload) | ✅ Configured |
| Auto-merge on PR pass | ✅ Configured |

## Transport Layer

| Feature                        | Status      | Notes                                              |
|--------------------------------|-------------|-----------------------------------------------------|
| SPI wiring                     | ✅ Verified |                                                     |
| `reset()`                      | ✅ Verified | Single-byte 0x00 command, 10ms delay                |
| `read8()`                      | ✅ Verified |                                                     |
| `read16()`                     | ✅ Verified |                                                     |
| `read32()`                     | ✅ Verified |                                                     |
| `write8()`                     | ✅ Verified | Confirmed via IOCON bit-flip read-back              |
| `write32()`                    | ✅ Verified |                                                     |
| SPI instruction encoding       | ✅ Verified | `cmd | (addr >> 8) & 0x0F`, then `addr & 0xFF`      |

## CAN Controller

| Feature                        | Status      | Notes                                              |
|--------------------------------|-------------|-----------------------------------------------------|
| CiCON address (0x000)          | ✅ Verified |                                                     |
| `setMode(MODE_CONFIG)`         | ✅ Verified | REQOP byte-write to CiCON+3                         |
| `setMode(MODE_INTERNAL_LB)`    | ✅ Verified | OPMOD confirms 2 after request                      |
| `setMode(MODE_NORMAL)`         | ✅ Verified | Two-node communication confirmed                    |
| `setMode(MODE_EXTERNAL_LB)`    | ✅ Verified | Real bus signals confirmed on oscilloscope          |
| `getMode()`                    | ✅ Verified | Reads OPMOD from CiCON+2 bits 7:5                   |
| 32-bit RMW of CiCON            | ❌ Broken   | Do not use — byte-level write only                  |

## Bit Timing

| Feature                              | Status      | Notes                                                                 |
|--------------------------------------|-------------|-----------------------------------------------------------------------|
| CiNBTCFG (nominal bit timing)        | ✅ Verified | 125 kbps @ 20 MHz: BRP=0 TSEG1=127 TSEG2=32 SJW=32 — scope: 24 µs first dominant run |
| CiDBTCFG (data bit timing)           | ✅ Verified | 2 Mbps @ 20 MHz: BRP=0 TSEG1=7 TSEG2=2 SJW=2                        |
| CiTDC (transmitter delay comp)       | ✅ Verified | TDCMOD=auto TDCO=8 @ 2 Mbps/20 MHz, loopback passes                  |
| Data rates 4/5 Mbps @ 20 MHz         | ✅ Verified | Auto-calculated, loopback passes                                      |
| Data rate 8 Mbps @ 20 MHz            | ❌ Not achievable | 20 MHz / 8 MHz = 2.5 TQ — RATE_NOT_ACHIEVABLE returned correctly |
| Data rates 1/2/4/5/8 Mbps (40 MHz presets via raw API) | ✅ Verified | configureRaw() + setDataBitTimingRaw(), loopback passes on 20 MHz hardware |
| OSC auto-detection                   | ✅ Verified | detectFsys() reads PLLEN+SCLKDIV from OSC reg — reports 20000000 Hz  |
| Calculated TX timeout                | ✅ Verified | Derived from bit timing at runtime — worst-case 64B frame × 3 attempts |
| NBTCFG/DBTCFG preset correction      | ✅ Verified | All presets now BRP=0, exact rates, 80% SP — verified by check_timing.py |

## FIFO / Messaging

| Feature                              | Status      | Notes                                                                 |
|--------------------------------------|-------------|-----------------------------------------------------------------------|
| FIFO register definitions            | ✅ Verified | Addresses confirmed on hardware                                       |
| TX FIFO configuration                | ✅ Verified | FIFO1=TX, PLSIZE_64, FSIZE=4, TXAT=3 attempts                        |
| RX FIFO configuration                | ✅ Verified | FIFO2=RX, PLSIZE_64, FSIZE=4                                         |
| TXQEN/STEF cleared in CiCON          | ✅ Verified | Byte-write to CiCON+2                                                |
| RTXAT enabled                        | ✅ Verified | CiCON byte 2 bit 0 — limits retransmissions via TXAT                 |
| RAM allocation for FIFOs             | ✅ Verified | UA1=0x000 (RAM 0x400), UA2=0x010 (RAM 0x410)                         |
| Send one frame                       | ✅ Verified | T0=0x123 FDF BRS DLC=8, TXREQ cleared, no errors                     |
| Receive a frame (internal loopback)  | ✅ Verified | Filter 0 accept-all→FIFO2, all fields match                          |
| Full loopback round-trip             | ✅ Verified | TX SID=0x123 FDF BRS DLC=8 data=0x01..0x08 received intact           |
| Multi-frame loopback (3 frames)      | ✅ Verified | SIDs 0x001 0x7FF 0x456 all OK at 2 Mbps                              |
| `setDataBitTiming()` runtime switch  | ✅ Verified | Config round-trip, restores previous mode, re-enables filter          |
| 64-byte payload (DLC=15)             | ✅ Verified | PLSIZE_64 on both FIFOs, dlcToLen() loop, all 64 bytes verified       |
| `available()` non-blocking check     | ✅ Verified | Polls FIFOSTA_TFNRFNIF without blocking                               |
| `receive(msg, timeoutMs)` overload   | ✅ Verified | Blocking receive with explicit timeout                                |
| TXABT/TXERR checked after transmit   | ✅ Verified | Returns false on no-ACK or bus error, not just on timeout             |

## API

| Feature                              | Status      | Notes                                                                 |
|--------------------------------------|-------------|-----------------------------------------------------------------------|
| `configure(nominalBps, dataBps, mode)` | ✅ Verified | Rate-based API — auto-detects FSYS, calculates all timing registers   |
| `setDataRate(dataBps)`               | ✅ Verified | Calculates before entering config mode — chip state unchanged on failure |
| `configureRaw(nbtcfg, dbtcfg, tdc, mode)` | ✅ Verified | Direct register control, bypasses auto-detection                 |
| `setDataBitTimingRaw(dbtcfg, tdc)`   | ✅ Verified | Direct register control for data rate                                 |
| `CanStatus` enum                     | ✅ Verified | OK / MODE_TIMEOUT / RATE_NOT_ACHIEVABLE / CLOCK_NOT_READY             |
| `getFsys()`                          | ✅ Verified | Returns detected FSYS in Hz after configure()                         |
| `readOsc()`                          | ✅ Verified | Returns raw OSC register value for diagnostics                        |
| `setFilter(index, id, mask, ext)`    | ✅ Verified | Disable→write OBJ+MASK→re-enable; safe in normal mode; routes to FIFO2 |
| `clearFilter(index)`                 | ✅ Verified | Writes 0x00 to filter byte — disables without touching OBJ/MASK         |
| Acceptance filter SID exact match    | ✅ Verified | 0x7EC passes, 0x123 dropped — loopback confirmed                        |
| Acceptance filter EID exact match    | ✅ Verified | 0x1C42017B passes, 0x18DAF101 dropped — loopback confirmed              |

## Two-Node (Real Bus)

| Feature                              | Status      | Notes                                                                 |
|--------------------------------------|-------------|-----------------------------------------------------------------------|
| Physical bus output (scope verified) | ✅ Verified | 125 kbps nominal / 2 Mbps data confirmed on oscilloscope             |
| Two-node MODE_NORMAL                 | ✅ Verified | A↔B bidirectional, 2/4/5/8 Mbps, 8B+64B payloads, no coordination   |
| No-ACK retry (RTXAT)                 | ✅ Verified | Chip retries 3× then clears TXREQ; app-level retry handles power-on race |

## SPEC-003 — Bus Error Detection and TX Error Detail

| Feature | Status | Notes |
|---|---|---|
| `CanTxResult` enum | ✅ Verified | OK / NoAck / BusError / FifoFull |
| `CanError` struct | ✅ Verified | tec, rec, txWarning, rxWarning, txPassive, rxPassive, busOff, rxOverflow |
| `transmit()` returns `CanTxResult` | ✅ Verified | Breaking change — all call sites updated |
| `getErrors()` | ✅ Verified | Reads CiTREC + CiRXOVIF |
| `hasErrors()` | ✅ Verified | Polls EWARN + TXBO + RXOVIF_FIFO2 |
| No-second-node TX failure | ✅ Verified | Returns BusError (TXERR) with floating bus; TEC > 0 confirmed |
| All repeated TX fail non-OK | ✅ Verified | 20 consecutive attempts all non-OK with no second node |

### Hardware observations
- With no bus connected (floating CANH/CANL): chip sets TXERR immediately on first bit → `BusError`
- With bus connected but no second node (termination present): chip exhausts 3 retries → `NoAck`
- TEC increments by ~8 per failed attempt; chip auto-recovers from bus-off (resets CiTREC)
- CiTREC, CiBDIAG0, CiBDIAG1 reset on every config-mode exit — only valid in Normal mode

## SPEC-004 — Interrupt-driven RX and Configurable FIFO Depth

| Feature | Status | Notes |
|---|---|---|
| `configure()` rxFifoDepth parameter | ✅ Verified | Default 16; clamped to 24 (hard max at PLSIZE_64 with FIFO1 depth=4) |
| FSIZE field encoding (depth-1) | ✅ Verified | FSIZE=15 for depth=16; 16 frames received without overflow |
| `configureRaw()` rxFifoDepth parameter | ✅ Verified | Same clamping logic |
| RX FIFO overflow detection | ✅ Verified | depth=4, 5 frames → CiFIFOSTA2.RXOVIF set; getErrors().rxOverflow=true |
| Overflow recovery | ✅ Verified | getErrors() clears RXOVIF via write-0 to CiFIFOSTA2; second call returns false |
| Two-node overflow | ✅ Verified | A depth=4, B bursts 5 frames; A confirms rxOverflow on real bus |
| INT pin (GPIO 34) ISR | ✅ Verified | available() returns true within 1 ms after TX; no polling loop needed |
| TFNRFNIE + RXIE enabled in config mode | ✅ Verified | CiFIFOCON2 bit 0 + CiINT byte 2 bit 1 set when intPin >= 0 |
| ISR is IRAM_ATTR, sets flag only | ✅ Verified | No SPI inside ISR; static trampoline via sIsrInstance |
| Polling fallback (intPin=-1) | ✅ Verified | available() polls FIFOSTA_TFNRFNIF as before; all existing tests pass |

### Hardware observations
- CiRXOVIF (0x028) is read-only — cleared by writing 0 to CiFIFOSTA2 (FIFO_STA(2))
- FSIZE field is depth-1: FSIZE=0 → 1 slot, FSIZE=15 → 16 slots, FSIZE=23 → 24 slots
- Max safe RX FIFO depth at PLSIZE_64 with FIFO1 depth=4: floor((2048-288)/72) = 24 slots
- INT pin fires within ~50 µs of frame arrival in internal loopback at 2 Mbps
- Static ISR trampoline (sIsrInstance) limits driver to one interrupt-enabled instance per program

| Example          | Status      | Notes                                                                 |
|------------------|-------------|-----------------------------------------------------------------------|
| single_node      | ✅ Verified | All assertions OK on COM4; depth=16 burst, overflow+recovery, INT pin |
| id_filter        | ✅ Verified | All assertions OK on COM4; no regressions |
| two_node         | ✅ Verified | All assertions OK on both nodes; overflow test A depth=4 B bursts 5 |

## SPEC-006 — Stop, Restart and Sleep/Wake Lifecycle

| Feature | Status | Notes |
|---|---|---|
| `stop()` | ✅ Verified | Enters MODE_CONFIG; saves previous mode; idempotent |
| `restart()` | ✅ Verified | Restores saved mode; loopback TX/RX intact after restart |
| `sleep()` | ✅ Verified | REQOP=001 (LPMEN=0); handshake: OPMOD=CONFIG + OSCDIS=1 |
| `wake()` | ✅ Verified | Clears OSC.OSCDIS (bit 2); waits OSCREADY; restores previous mode |
| `configure()` sleep guard | ✅ Verified | Clears OSCDIS before reset() if chip is sleeping |

### Hardware observations
- OSCDIS is bit 2 of OSC byte 0 (not bit 3 as initially assumed)
- Sleep handshake: OPMOD reads CONFIG (4) and OSCDIS reads 1 within ~1 ms of REQOP=001
- Wake by clearing OSCDIS: OSCREADY asserts within 1 ms; previous mode restored cleanly
- configure() called after sleep() without wake(): OSCDIS guard clears it, reset proceeds normally
- All loopback assertions pass after stop/restart and sleep/wake cycles

| Example          | Status      | Notes                                                                 |
|------------------|-------------|-----------------------------------------------------------------------|
| single_node      | ✅ Verified | All assertions OK on COM4; stop/restart/sleep/wake all pass |
| id_filter        | ✅ Verified | All assertions OK on COM4; no regressions |
| two_node         | ✅ Verified | All assertions OK on both nodes; no regressions |
| unit tests       | ✅ Verified | 50/50 passing |

| `configure()` enableTimestamp parameter | ✅ Verified | Default false; sets RXTSEN in FIFO2, enables CiTBC after setMode() |
| `configureRaw()` enableTimestamp parameter | ✅ Verified | Same behaviour |
| CiTBC free-running counter | ✅ Verified | TBCEN=1 written after mode transition; TBCPRE=0 = 50 ns/count at 20 MHz |
| RXTSEN in CiFIFOCON2 | ✅ Verified | Bit 5; config-mode-only; slot grows from 72 to 76 bytes |
| RX message object layout with RXTSEN | ✅ Verified | R0(+0) R1(+4) R2/timestamp(+8) payload(+12) — timestamp before payload |
| timestamp > 0 after loopback | ✅ Verified | Confirmed on hardware |
| timestamp delta ~10 ms | ✅ Verified | Observed 251124 counts (~12.6 ms incl. frame time) |
| timestamp=0 when enableTimestamp=false | ✅ Verified | Regression check passes |
| payload intact with timestamp enabled | ✅ Verified | payloadOffset=12 when mTimestamp=true |
| Max RX FIFO depth with timestamp | ✅ Verified | Clamped to 23 (76-byte slots); 24 without |
| `transmit()` returns NoAck in MODE_LISTEN | ✅ Verified | Early return before any FIFO access |
| MODE_LISTEN two-node: A receives, B no errors | ✅ Verified | A received frames; B TEC=0, not bus-off |

### Hardware observations
- CiTSCON must be written AFTER setMode() exits config mode — writing TBCEN in config mode
  does not survive the mode transition (register resets on config-mode exit)
- RX message object layout: timestamp (R2) is at offset +8, payload starts at +12 when RXTSEN=1.
  This is the opposite of what the spec originally assumed (payload-then-timestamp).
- In MODE_LISTEN, Node B (MODE_NORMAL) transmits with NoAck result — expected because A sends
  no ACK. B does not go bus-off. A receives frames with TEC=0.
- Timestamp delta for 10 ms delay: 251124 counts = 12.6 ms (includes ~2.6 ms frame time
  at 125 kbps nominal / 2 Mbps data in loopback)

| Example          | Status      | Notes                                                                 |
|------------------|-------------|-----------------------------------------------------------------------|
| single_node      | ✅ Verified | All assertions OK on COM4; timestamp, payload, delta, listen-only |
| id_filter        | ✅ Verified | All assertions OK on COM4; no regressions |
| two_node         | ✅ Verified | All assertions OK on both nodes; listen-only A/B test |

## SPEC-009 — CLKO Output Pin Configuration

| Feature | Status | Notes |
|---|---|---|
| `CanConfig.clkoDivider` field | ✅ Verified | Default 0 = leave at reset default; 1/2/4/10 = active |
| CLKODIV encoding (OSC bits 6:5) | ✅ Verified | 00=÷1, 01=÷2, 10=÷4, 11=÷10 (DS20006027B Register 3-1, page 16) |
| Invalid divider returns RATE_NOT_ACHIEVABLE | ✅ Verified | clkoDivToReg() returns 0xFF for non-{1,2,4,10} values |
| CLKODIV written after reset, before detectFsys() | ✅ Verified | OSC byte 0 read-modify-write in config mode |
| No regression on existing callers | ✅ Verified | All three suites pass; clkoDivider=0 leaves OSC unchanged |
| `configureRaw()` also accepts clkoDivider | ✅ Verified | Same write path |

### Hardware observations
- CLKO pin is always driven at reset (CLKODIV=11=÷10 by default). There is no CLKOEN bit.
- CLKODIV is R/W with no config-mode restriction, but we write it immediately after reset() before setMode(), matching the Microchip reference sequence (MCP25XXFD canfdspi API Example 3-1: OscillatorControlSet called right after Reset).
- Writing OSC byte 0 at any other point in the configure sequence (in config mode after setMode(), or after exiting config mode) disables the RX path on this hardware — frames transmit OK but never arrive in FIFO2. Root cause is undocumented chip behaviour; the Microchip reference sequence avoids it by writing OSC immediately after reset before any other register access.
- Loopback (MODE_INTERNAL_LB) does not work after an OSC write on this hardware. Functional verification uses two_node (MODE_NORMAL, real bus): A configures with clkoDivider=10, transmits to B, B receives — confirmed OK.

| Suite | Status | Notes |
|---|---|---|
| single_node | ✅ Verified | configure() OK + mode confirmed for each divider; invalid value rejected |
| two_node | ✅ Verified | A configures clkoDivider=10, transmits to B, B receives — real bus functional |
| id_filter | ✅ Verified | No regressions |
| unit tests | ✅ Verified | 100/100 passing, 100% lines/functions |

## SPEC-010 — Dual INT Pin Support (INT0 / INT1)

| Feature | Status | Notes |
|---|---|---|
| Constructor accepts `int0Pin` and `int1Pin` | ✅ Verified | Both default to `NO_INT_PIN`; existing callers unaffected |
| `int0Pin` ignored (TX-only) | ✅ Verified | DS20006027B page 18: INT0 asserts on TXIF only — not useful for RX |
| `int1Pin` activates INT1 as RX interrupt | ✅ Verified | IOCON byte 3 PM1=0 written when `int1Pin >= 0` and `intPin < 0` |
| ISR attached to `int1Pin` when `intPin` is NC | ✅ Verified | `available()` returns true within 1 ms via INT1 path |
| `available()` false after drain (INT1 path) | ✅ Verified | `mRxPending` cleared by `receive()` |
| Existing `intPin` behaviour unchanged | ✅ Verified | All prior single_node/id_filter/two_node assertions pass |

### Hardware observations
- INT1 is RX-only (CiINT.RXIF & RXIE). INT0 is TX-only (CiINT.TXIF & TXIE). INT is the combined pin.
- IOCON reset default: PM1=1, PM0=1 (both pins are GPIO). Must explicitly clear PM1 to activate INT1.
- IOCON must be written byte-by-byte (DS20006027B page 70 note). `write8(REG_IOCON + 3, ...)` used.
- Verified on our board by wiring GPIO 34 (normally INT) as `int1Pin` with `intPin=NO_INT_PIN`. ISR fires correctly.
- T-2CAN FD board (INT=NC, INT1 wired) not available for direct verification; register path confirmed correct.

| Suite | Status | Notes |
|---|---|---|
| single_node | ✅ Verified | INT1 path: available() within 1 ms, drain clears flag; all prior assertions pass |
| id_filter | ✅ Verified | No regressions |
| two_node | ✅ Verified | No regressions |
| unit tests | ✅ Verified | 100/100 passing |



| Gap | Resolution |
|---|---|
| SPEC-007 `resetFilters()` uncalled | Added to `id_filter` harness — sets filters 0+1, calls `resetFilters()`, verifies all three IDs pass |
| SPEC-007 stop→sleep→wake sequence | Added to `single_node` — verifies `mStopPrevMode`/`mSleepPrevMode` independence |
| SPEC-009 CLKO register readback | Added to `single_node` — iterates dividers 1/2/4/10, verifies `CanStatus::OK` + loopback per divider, confirms invalid value 3 returns `RATE_NOT_ACHIEVABLE` |
| SPEC-003 disconnected-bus TX error | Not automatable — requires physical bus disconnection. Accepted as manual-only gap. |



| Feature | Status | Notes |
|---|---|---|
| `MODE_CLASSIC` constant | ✅ Verified | REQOP=110 (0x06) — confirmed DS20006027B page 27 |
| `configure(nominal, 0, MODE_CLASSIC)` | ✅ Verified | Chip enters Normal CAN 2.0 mode; OPMOD=6 confirmed |
| Data rate parameter ignored | ✅ Verified | `dataBps=0` passes nominal rate to calcBitTiming(); no RATE_NOT_ACHIEVABLE |
| `transmit()` FD frame rejected | ✅ Verified | Returns `CanTxResult::InvalidMode` before touching chip |
| `setDataRate()` in classic mode | ✅ Verified | Returns `CanStatus::INVALID_MODE` immediately |
| Classic frame loopback (MODE_INTERNAL_LB, fdf=false) | ✅ Verified | fdf=false received intact; all 8 bytes match |
| Two-node classic CAN (500 kbps) | ✅ Verified | A↔B exchange 10 frames each; all received |
| `CanStatus::INVALID_MODE` | ✅ Verified | New enum value; returned by `setDataRate()` in classic mode |
| `CanTxResult::InvalidMode` | ✅ Verified | New enum value; returned by `transmit()` with fdf=true in classic mode |

### Hardware observations
- MODE_CLASSIC is a real-bus mode — no internal loopback equivalent. Single-node tests use MODE_INTERNAL_LB with fdf=false frames to verify classic frame handling.
- The chip ignores FDF/BRS/ESI bits in TX objects in Normal CAN 2.0 mode (REFMANUAL page 10) — the API guard at transmit() prevents silent coercion.
- Two-node classic exchange at 500 kbps: 10 frames A→B and 10 frames B→A, all received correctly.
- `configure(500000, 0, MODE_CLASSIC)` works cleanly — 0 is substituted with nominalBps inside configure() before calling calcBitTiming().

| Suite | Status | Notes |
|---|---|---|
| single_node | ✅ Verified | MODE_CLASSIC entry, FD rejection, setDataRate guard, classic loopback |
| id_filter | ✅ Verified | No regressions |
| two_node | ✅ Verified | 10-frame classic exchange A↔B on real bus |
| unit tests | ✅ Verified | 100/100 passing, 100% lines/functions |
