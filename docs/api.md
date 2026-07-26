# API Reference

Complete reference for every public type and method in `mcp2518fd_can.h`.

For a quick-start see the [README](../README.md). For hardware wiring see [hardware.md](hardware.md).

---

## Types

### CanMsg

Represents a single CAN or CAN FD frame. Used for both transmit and receive.

```cpp
struct CanMsg {
    uint32_t id        = 0;
    bool     ext       = false;
    bool     fdf       = false;
    bool     brs       = false;
    uint8_t  dlc       = 0;
    uint32_t timestamp = 0;
    uint8_t  data[64]  = {};
};
```

| Field | Type | Description |
|---|---|---|
| `id` | `uint32_t` | Frame identifier. 11-bit SID when `ext=false`; 29-bit EID when `ext=true`. |
| `ext` | `bool` | `false` = standard 11-bit frame. `true` = extended 29-bit frame. |
| `fdf` | `bool` | `true` = CAN FD frame. `false` = Classic CAN frame. |
| `brs` | `bool` | `true` = switch to data bit rate in the payload phase. Only meaningful when `fdf=true`. |
| `dlc` | `uint8_t` | Data Length Code (0–15). Use `dlcToLen(dlc)` to get the byte count. |
| `timestamp` | `uint32_t` | Hardware timestamp on received frames (TBC counter counts, 50 ns per count at 20 MHz). Zero when timestamping is not enabled. |
| `data[64]` | `uint8_t[]` | Payload bytes. Valid bytes are `data[0]` through `data[dlcToLen(dlc)-1]`. |

---

### CanStatus

Returned by `configure()`, `configureRaw()`, `setDataRate()`, `stop()`, `restart()`, `sleep()` and `wake()`.

| Value | Meaning |
|---|---|
| `CanStatus::OK` | Success. |
| `CanStatus::MODE_TIMEOUT` | Chip did not confirm the requested mode within the timeout. |
| `CanStatus::RATE_NOT_ACHIEVABLE` | Target bit rate cannot be reached at the detected oscillator frequency. Chip state is unchanged. |
| `CanStatus::CLOCK_NOT_READY` | OSC register shows clock not stable after reset. |
| `CanStatus::INVALID_MODE` | Operation not valid in the current mode (e.g. `setDataRate()` in `MODE_CLASSIC`). |

Always check for `CanStatus::OK` before transmitting. Any other value means the chip is not in the requested mode.

---

### CanTxResult

Returned by `transmit()`.

| Value | Meaning |
|---|---|
| `CanTxResult::OK` | Frame transmitted and ACKed by at least one other node. |
| `CanTxResult::NoAck` | Chip retried 3 times, no ACK received. Other node absent or bus disconnected. |
| `CanTxResult::BusError` | TXERR set — bit error, stuff error, or floating bus. |
| `CanTxResult::FifoFull` | TX FIFO had no space (TFNRFNIF was clear). |
| `CanTxResult::InvalidMode` | Operation not valid in the current mode (e.g. FD frame in `MODE_CLASSIC`). |

---

### CanError

Returned by `readAndClearErrors()`.

| Field | Type | Description |
|---|---|---|
| `tec` | `uint8_t` | Transmit error counter (0–255). |
| `rec` | `uint8_t` | Receive error counter (0–255). |
| `txWarning` | `bool` | TEC ≥ 96. |
| `rxWarning` | `bool` | REC ≥ 96. |
| `txPassive` | `bool` | TEC ≥ 128 — node is error-passive on TX. |
| `rxPassive` | `bool` | REC ≥ 128 — node is error-passive on RX. |
| `busOff` | `bool` | TEC > 255 — node is bus-off and not transmitting. |
| `rxOverflow` | `bool` | At least one RX FIFO overflow since last call. Cleared as a side effect of this call. |

---

### CanConfig

Optional settings passed as the fourth argument to `configure()` or `configureRaw()`. All fields have safe defaults — omitting it gives the same behaviour as the three-argument form.

```cpp
struct CanConfig {
    uint8_t rxFifoDepth     = 16;
    bool    enableTimestamp = false;
};
```

| Field | Default | Description |
|---|---|---|
| `rxFifoDepth` | `16` | RX FIFO slot count. Range: 1–24 without timestamps, 1–23 with timestamps. Clamped automatically. |
| `enableTimestamp` | `false` | Attach a 32-bit hardware timestamp to every received frame. Resolution: 1 FSYS clock (50 ns at 20 MHz). |

---

## MCP2518Driver

### Constructor

```cpp
MCP2518Driver(SPIClass& spi, uint8_t csPin, int8_t intPin = NO_INT_PIN);
```

| Parameter | Description |
|---|---|
| `spi` | An initialised `SPIClass` instance. Call `spi.begin(...)` before `configure()`. |
| `csPin` | GPIO connected to the MCP2518FD CS pin. |
| `intPin` | GPIO connected to the MCP2518FD INT pin (active-low). Pass `NO_INT_PIN` (default) for polling-only mode. |

---

### configure()

```cpp
CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode);
CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, const CanConfig& cfg);
```

Resets the chip, auto-detects the oscillator frequency, calculates all bit timing registers from the target rates, configures FIFOs and a catch-all acceptance filter, then enters the requested mode.

| Parameter | Description |
|---|---|
| `nominalBps` | Nominal (arbitration) bit rate in bits per second. E.g. `500000` for 500 kbps. |
| `dataBps` | Data bit rate in bits per second. E.g. `2000000` for 2 Mbps. Must be ≥ `nominalBps`. |
| `mode` | Operating mode. See [Mode constants](#mode-constants). |
| `cfg` | Optional. FIFO depth and timestamp settings. |

Returns `CanStatus::OK` on success. Returns `CanStatus::RATE_NOT_ACHIEVABLE` if either rate cannot be achieved at the detected oscillator frequency — the chip is left in reset in this case.

Safe to call again after `stop()` or `sleep()` to fully reinitialise the chip.

---

### setDataRate()

```cpp
CanStatus setDataRate(uint32_t dataBps);
```

Changes the data bit rate at runtime without disturbing the nominal rate or current mode. Calculates timing before touching the chip — if the rate is not achievable the chip state is completely unchanged.

Returns `CanStatus::INVALID_MODE` immediately if the driver is in `MODE_CLASSIC` — there is no data phase in classic CAN.

| Parameter | Description |
|---|---|
| `dataBps` | New data bit rate in bits per second. |

---

### transmit()

```cpp
CanTxResult transmit(const CanMsg& msg);
```

Transmits one frame. Blocks until the chip confirms transmission or reports an error (typically < 1 ms at 500 kbps nominal). Returns `CanTxResult::OK` when the frame was ACKed.

In `MODE_LISTEN` returns `CanTxResult::NoAck` immediately without accessing the FIFO.

In `MODE_CLASSIC` returns `CanTxResult::InvalidMode` immediately if `msg.fdf=true`. Classic CAN frames (`fdf=false`) transmit normally.

---

### available()

```cpp
bool available();
```

Returns `true` if at least one frame is waiting in the RX FIFO. Non-blocking.

With an INT pin configured, this reads a flag set by the ISR — no SPI transaction. Without an INT pin, it reads the FIFO status register over SPI.

---

### receive()

```cpp
bool receive(CanMsg& msg, uint32_t timeoutMs = 0);
```

Reads one frame from the RX FIFO into `msg`.

| Parameter | Description |
|---|---|
| `msg` | Output. Populated with the received frame. |
| `timeoutMs` | `0` (default): non-blocking, returns `false` immediately if no frame is available. `> 0`: blocks until a frame arrives or the timeout expires. |

Returns `true` if a frame was received, `false` otherwise.

---

### readAndClearErrors()

```cpp
CanError readAndClearErrors();
```

Reads TEC/REC counters and all error flags. Clears `rxOverflow` as a side effect. Use this when you need the full error picture. For a cheaper yes/no check use `hasErrors()`.

---

### hasErrors()

```cpp
bool hasErrors();
```

Returns `true` if TEC or REC ≥ 96, bus-off, or RX overflow. Cheaper than `readAndClearErrors()` — reads a single register. Does not clear any flags.

---

### setFilter()

```cpp
void setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext);
```

Configures acceptance filter slot `index` (0–31). All matched frames are routed to the RX FIFO. Safe to call in normal mode — the filter is disabled, updated, then re-enabled atomically.

| Parameter | Description |
|---|---|
| `index` | Filter slot (0–31). |
| `id` | Frame ID to match. 11-bit SID when `ext=false`; 29-bit EID when `ext=true`. |
| `mask` | Bit mask. A `1` bit means that bit of `id` must match. `0` means don't care. Pass `0x7FF` (SID) or `0x1FFFFFFF` (EID) for an exact match. Pass `0` for a catch-all. |
| `ext` | `false` = match standard frames only. `true` = match extended frames only. |

Filter slot 0 is configured as a catch-all by `configure()`. Overwrite it with `setFilter(0, ...)` to restrict what is received.

---

### clearFilter()

```cpp
void clearFilter(uint8_t index);
```

Disables filter slot `index`. Frames that would have matched this filter are no longer received (unless another filter matches them).

---

### resetFilters()

```cpp
void resetFilters();
```

Restores the catch-all filter on slot 0 and disables slots 1–31. Equivalent to the filter state after `configure()`.

---

### stop()

```cpp
CanStatus stop();
```

Enters Configuration mode, halting all TX and RX. The current mode is saved. Call `restart()` to resume. Idempotent — calling `stop()` when already stopped is safe.

---

### restart()

```cpp
CanStatus restart();
```

Returns to the mode that was active before `stop()`. TX and RX resume immediately.

---

### sleep()

```cpp
CanStatus sleep();
```

Enters low-power Sleep mode. The oscillator is disabled. The chip can be woken by bus activity or by calling `wake()`. The current mode is saved.

---

### wake()

```cpp
CanStatus wake();
```

Exits Sleep mode, waits for the oscillator to stabilise, then restores the mode that was active before `sleep()`.

---

### getMode()

```cpp
uint8_t getMode();
```

Returns the current operating mode. Compare against the `MODE_*` constants below.

---

### getFsys()

```cpp
uint32_t getFsys() const;
```

Returns the oscillator frequency in Hz as detected by `configure()`. Returns `0` if `configure()` has not been called. Typical values: `20000000` or `40000000`.

---

### configureRaw()

```cpp
CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode);
CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode, const CanConfig& cfg);
```

Direct register control for non-standard rates or custom oscillators. Bypasses auto-detection and timing calculation — you supply the raw register words. Preset constants for common rates are in `mcp2518fd_presets.h`.

Note: `mFsys` is not set by this path. If you subsequently call `setDataRate()`, it will use 20 MHz as a fallback. Call `configure()` first if you need FSYS detection, then switch to raw timing.

---

### setDataBitTimingRaw()

```cpp
CanStatus setDataBitTimingRaw(uint32_t dbtcfg, uint32_t tdcfg);
```

Changes the data bit timing registers directly at runtime. Same semantics as `setDataRate()` but accepts raw register words instead of a bit rate.

---

## Mode constants

Defined in `mcp2518fd_registers.h`, included automatically via `mcp2518fd_can.h`.

| Constant | Value | Description |
|---|---|---|
| `MODE_NORMAL` | 0 | Normal CAN FD operation. Requires at least one other node to ACK frames. |
| `MODE_SLEEP` | 1 | Low-power sleep. Use `sleep()` / `wake()` rather than passing this to `configure()`. |
| `MODE_INTERNAL_LB` | 2 | Internal loopback. TX frames loop back to RX internally. No bus signals. No second node required. |
| `MODE_LISTEN` | 3 | Listen-only. Receives all frames, sends no ACK. Does not affect bus error counters on other nodes. |
| `MODE_CONFIG` | 4 | Configuration mode. Used internally by the driver. |
| `MODE_EXTERNAL_LB` | 5 | External loopback. Drives real signals on CANH/CANL via the transceiver and self-ACKs. No second node required. Useful for oscilloscope measurements. |
| `MODE_CLASSIC` | 6 | Classic CAN 2.0B mode. No CAN FD frames, no BRS. Use when the bus has classic-only nodes. `transmit()` with `fdf=true` returns `CanTxResult::InvalidMode`. `setDataRate()` returns `CanStatus::INVALID_MODE`. Pass `0` or any value for `dataBps` — it is ignored. |
| `MODE_RESTRICTED` | 7 | Restricted operation mode. |

---

## Helper functions

These are free functions in `mcp2518fd_timing.h`, included automatically.

### dlcToLen()

```cpp
constexpr uint8_t dlcToLen(uint8_t dlc);
```

Converts a 4-bit DLC code to a byte count. DLC 0–8 map 1:1. DLC 9=12, 10=16, 11=20, 12=24, 13=32, 14=48, 15=64.

```cpp
uint8_t len = dlcToLen(msg.dlc);
for (int i = 0; i < len; i++) { /* process msg.data[i] */ }
```

---

## Preset constants

`mcp2518fd_presets.h` (included automatically) defines pre-computed register words for common rates and oscillator frequencies. Use these with `configureRaw()` and `setDataBitTimingRaw()` when you need direct register control.

```cpp
// Example: 500 kbps nominal / 2 Mbps data on a 40 MHz oscillator
can.configureRaw(NBTCFG_500K_40MHZ, DBTCFG_2M_40MHZ, TDC_2M_40MHZ, MODE_NORMAL);
```

All presets use BRP=0, exact rates, 80% sample point (75% for 5 Mbps). Prefer `configure(nominalBps, dataBps, mode)` for standard rates — it selects the correct preset automatically.
