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

| Example          | Status      | Notes                                                                 |
|------------------|-------------|-----------------------------------------------------------------------|
| single_node      | ✅ Verified | 42 assertions, all OK on COM4; config, bitrates, raw API              |
| id_filter        | ✅ Verified | 15 assertions, all OK on COM4; SID/EID exact, range, multi, catch-all |
| two_node         | ✅ Verified | Full bidirectional test, all assertions OK on both nodes              |
| walkie_talkie    | ✅ Verified | Text chat working between two boards over real bus                    |
| scope_loopback   | ✅ Verified | FSYS=20 MHz detected, 24 µs first dominant run @ 125 kbps scope-confirmed |
| bus_monitor      | ✅ Verified | Autonomous boot, continuous TX/RX both nodes, RX_ERR=0 sustained. Redesigned to compile-time NODE_SID (node_a/node_b envs) — no serial input required |
