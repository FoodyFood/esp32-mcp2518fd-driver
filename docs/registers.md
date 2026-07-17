# Register Reference

Chip: MCP2518FD — 40 MHz oscillator, SPI mode 0,0, little-endian byte order.

---

## CiCON — CAN Control Register (0x000)

| Bits  | Field  | Description                        |
|-------|--------|------------------------------------|
| 26:24 | REQOP  | Requested operating mode           |
| 23:21 | OPMOD  | Current operating mode (read-only) |

### Operating Modes

| Value | Name          | Constant           |
|-------|---------------|--------------------|
| 0     | Normal FD     | MODE_NORMAL        |
| 1     | Sleep         | MODE_SLEEP         |
| 2     | Internal LB   | MODE_INTERNAL_LB   |
| 3     | Listen Only   | MODE_LISTEN        |
| 4     | Configuration | MODE_CONFIG        |
| 5     | External LB   | MODE_EXTERNAL_LB   |
| 6     | Classic CAN   | MODE_CLASSIC       |
| 7     | Restricted    | MODE_RESTRICTED    |

### Byte Map

| Byte offset | Bits   | Contains         |
|-------------|--------|------------------|
| CiCON+0     | 7:0    | Lower config     |
| CiCON+1     | 15:8   | Lower config     |
| CiCON+2     | 23:16  | OPMOD [7:5], TXQEN [bit4], STEF [bit3] |
| CiCON+3     | 31:24  | REQOP [2:0]      |

---

## CiNBTCFG — Nominal Bit Time Configuration (0x004)

| Bits  | Field  | Description                        |
|-------|--------|------------------------------------|
| 31:24 | BRP    | Baud rate prescaler                |
| 23:16 | TSEG1  | Time segment 1                     |
| 14:8  | TSEG2  | Time segment 2                     |
| 6:0   | SJW    | Synchronisation jump width         |

---

## CiDBTCFG — Data Bit Time Configuration (0x008)

| Bits  | Field  | Description                        |
|-------|--------|------------------------------------|
| 31:24 | BRP    | Baud rate prescaler                |
| 20:16 | TSEG1  | Time segment 1                     |
| 11:8  | TSEG2  | Time segment 2                     |
| 3:0   | SJW    | Synchronisation jump width         |

---

## CiTDC — Transmitter Delay Compensation (0x00C)

| Bits  | Field   | Description                        |
|-------|---------|-------------------------------------|
| 17:16 | TDCMOD  | TDC mode (0=disabled, 1=manual, 2=auto) |
| 13:8  | TDCO    | TDC offset                         |
| 5:0   | TDCV    | TDC value (read-only in auto mode) |

---

## CiFIFOCONm — FIFO m Control (base 0x05C, stride 0x0C)

| Bits  | Field   | Description                             |
|-------|---------|-----------------------------------------|
| 31:29 | PLSIZE  | Payload size (see table below)          |
| 28:24 | FSIZE   | FIFO depth (0 = 1 message deep)         |
| 10    | FRESET  | Set by HW in config mode, cleared on exit — do not write |
| 9     | TXREQ   | Set to request transmission (TX only)   |
| 8     | UINC    | Increment head/tail pointer             |
| 7     | TXEN    | 1 = TX FIFO, 0 = RX FIFO (config only) |

### Payload Size (PLSIZE)

| Value | Bytes |
|-------|-------|
| 0     | 8     |
| 1     | 12    |
| 2     | 16    |
| 3     | 20    |
| 4     | 24    |
| 5     | 32    |
| 6     | 48    |
| 7     | 64    |

### Verified reset values (in config mode)

| Register   | Address | Value      | Notes                        |
|------------|---------|------------|------------------------------|
| CiFIFOCON1 | 0x05C   | 0x00000480 | TXEN=1, FRESET=1 (HW-set)    |
| CiFIFOCON2 | 0x068   | 0x00000400 | TXEN=0, FRESET=1 (HW-set)    |

---

## SFR Addresses

| Register  | Address |
|-----------|---------|
| OSC       | 0xE00   |
| IOCON     | 0xE04   |
| CRC       | 0xE08   |
| ECCCON    | 0xE0C   |
| ECCSTAT   | 0xE10   |
| DEVID     | 0xE14   |

---

## RAM

| Item       | Detail                              |
|------------|-------------------------------------|
| Base       | 0x400                               |
| Size       | 2048 bytes                          |
| Usage      | FIFO message objects, allocated by firmware |
