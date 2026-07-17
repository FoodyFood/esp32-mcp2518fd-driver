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
| CiNBTCFG (nominal bit timing)  | 🔲 Not started |                                               |
| CiDBTCFG (data bit timing)     | 🔲 Not started |                                               |
| CiTDC (transmitter delay comp) | 🔲 Not started |                                               |

## FIFO / Messaging

| Feature                        | Status       | Notes                                           |
|--------------------------------|--------------|-------------------------------------------------|
| RAM initialisation             | 🔲 Not started |                                               |
| TX FIFO configuration          | 🔲 Not started |                                               |
| RX FIFO configuration          | 🔲 Not started |                                               |
| Send one frame                 | 🔲 Not started |                                               |
| Receive one frame              | 🔲 Not started |                                               |
| Loopback frame verify          | 🔲 Not started |                                               |
