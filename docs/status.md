# Verified Status

Each item below has been tested on real hardware and confirmed working.

## Transport Layer

| Feature                        | Status    | Notes                                              |
|--------------------------------|-----------|----------------------------------------------------|
| SPI wiring                     | ✅ Verified |                                                   |
| `reset()`                      | ✅ Verified | Single-byte 0x00 command, 10ms delay               |
| `read8()`                      | ✅ Verified |                                                   |
| `read16()`                     | ✅ Verified |                                                   |
| `read32()`                     | ✅ Verified |                                                   |
| `write8()`                     | ✅ Verified | Confirmed via IOCON bit-flip read-back             |
| `write32()`                    | ✅ Verified |                                                   |
| SPI instruction encoding       | ✅ Verified | `cmd | (addr >> 8) & 0x0F`, then `addr & 0xFF`     |

## CAN Controller

| Feature                        | Status    | Notes                                              |
|--------------------------------|-----------|----------------------------------------------------|
| CiCON address (0x000)          | ✅ Verified |                                                   |
| `setMode(MODE_CONFIG)`         | ✅ Verified | REQOP byte-write to CiCON+3                        |
| `setMode(MODE_INTERNAL_LB)`    | ✅ Verified | OPMOD confirms 2 after request                     |
| `getMode()`                    | ✅ Verified | Reads OPMOD from CiCON+2 bits 7:5                  |
| 32-bit RMW of CiCON            | ❌ Broken  | Do not use — byte-level write only                 |

## Bit Timing

| Feature                        | Status       | Notes                                           |
|--------------------------------|--------------|-------------------------------------------------|
| CiNBTCFG (nominal bit timing)  | ✅ Verified  | BRP=0 TSEG1=30 TSEG2=7 SJW=7 → 125kbps @ 40MHz |
| CiDBTCFG (data bit timing)     | ✅ Verified  | BRP=0 TSEG1=14 TSEG2=3 SJW=3                   |
| CiTDC (transmitter delay comp) | 🔲 Not started |                                               |

## FIFO / Messaging

| Feature                        | Status       | Notes                                           |
|--------------------------------|--------------|-------------------------------------------------|
| FIFO register definitions      | ✅ Verified  | Step 1 — addresses confirmed on hardware. CiTXQCON=0x00600080 (TXEN=1 always), CiFIFOCON1=0x00600000, FIFO_CON() helper verified |
| TX FIFO configuration          | ✅ Verified  | Step 2 — FIFO1=TX (TXEN=1, PLSIZE=0, FSIZE=0), readback 0x00000480 (FRESET set by HW) |
| RX FIFO configuration          | ✅ Verified  | Step 2 — FIFO2=RX (all zeros), readback 0x00000400 (FRESET set by HW) |
| TXQEN/STEF cleared in CiCON    | ✅ Verified  | Step 2 — byte-write to CiCON+2, confirmed TXQEN=0 STEF=0 |
| RAM initialisation             | 🔲 Not started | Step 3                                        |
| RAM allocation for FIFOs       | ✅ Verified  | Step 3 — UA1=0x000 (RAM 0x400), UA2=0x010 (RAM 0x410). UA is offset from RAM base, not absolute address |
| Send one frame                 | ✅ Verified  | Step 4 — T0=0x123 FDF BRS DLC=8, TXREQ cleared, no errors. CiFIFOSTA1=0x00000007 before and after |
| Receive one frame              | 🔲 Not started | Step 6                                        |
| Loopback frame verify          | 🔲 Not started | Step 6                                        |
