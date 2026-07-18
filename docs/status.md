# Verified Status

Each item below has been tested on real hardware and confirmed working.

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
| CiNBTCFG (nominal bit timing)        | ✅ Verified | 125 kbps @ 40MHz: BRP=7 TSEG1=31 TSEG2=8 SJW=8 — scope verified     |
| CiDBTCFG (data bit timing)           | ✅ Verified | 2 Mbps @ 40MHz: BRP=0 TSEG1=18 TSEG2=1 SJW=1                        |
| CiTDC (transmitter delay comp)       | ✅ Verified | TDCMOD=auto TDCO=19 @ 2 Mbps, loopback passes                        |
| Data rates 4/5/8 Mbps                | ✅ Verified | DBTCFG+TDC presets derived from datasheet, loopback passes on 2 boards |
| Calculated TX timeout                | ✅ Verified | Derived from bit timing at runtime — worst-case 64B frame × 3 attempts |
| NBTCFG preset correction             | ✅ Verified | Original presets were 8× too fast; corrected and scope-verified       |

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

## Two-Node (Real Bus)

| Feature                              | Status      | Notes                                                                 |
|--------------------------------------|-------------|-----------------------------------------------------------------------|
| Physical bus output (scope verified) | ✅ Verified | 125 kbps nominal / 2 Mbps data confirmed on oscilloscope             |
| Two-node MODE_NORMAL                 | ✅ Verified | A↔B bidirectional, 2/4/5/8 Mbps, 8B+64B payloads, no coordination   |
| No-ACK retry (RTXAT)                 | ✅ Verified | Chip retries 3× then clears TXREQ; app-level retry handles power-on race |

## Examples

| Example          | Status      | Notes                                                                 |
|------------------|-------------|-----------------------------------------------------------------------|
| loopback         | ✅ Verified | 34 assertions, all OK on COM3 and COM4                                |
| two_node         | ✅ Verified | Full bidirectional test, all assertions OK on both nodes              |
| walkie_talkie    | ✅ Verified | Text chat working between two boards over real bus                    |
| scope_loopback   | ✅ Built    | Not yet run on hardware                                               |
| bus_monitor      | ✅ Built    | Not yet run on hardware                                               |
