# esp32-mcp2518fd-driver — Gaps Found During T-2Can FD Integration

Tested against **v1.1.3** on a LilyGo T-2Can FD (ESP32-S3, MCP2518FD, 40MHz crystal, no PLL).

---

## Issue 1 — `getFsys()` returns 0 after `configureRaw()`

`configureRaw()` bypasses `detectFsys()`, so `mFsys` is never populated. Any subsequent call to `getFsys()` returns 0 even though the chip is running correctly.

**Workaround:** we ignore the return value of `getFsys()` after `configureRaw()`.

**Suggested fix:** either populate `mFsys` from the caller-supplied timing in `configureRaw()`, or have `getFsys()` read the OSC register directly instead of returning the cached value.

---

## Issue 2 — No way to specify oscillator frequency in `configure()`

The driver auto-detects Fsys by reading the PLLEN bit in the OSC register: 40MHz if PLL is enabled, 20MHz otherwise. Boards with a 40MHz crystal and PLL disabled (e.g. T-2Can FD) are silently misconfigured — the driver calculates bit timing for 20MHz while the chip runs at 40MHz, producing a 2× rate error with no error reported.

**Workaround:** we pre-calculated the correct register values for 40MHz ourselves and called `configureRaw()` directly:

```cpp
// 40MHz, 500kbps nominal / 1Mbps data
#define T2CAN_NBTCFG  0x003F1010UL
#define T2CAN_DBTCFG  0x001F0808UL
#define T2CAN_TDCFG   0x00022000UL
can.configureRaw(T2CAN_NBTCFG, T2CAN_DBTCFG, T2CAN_TDCFG, MODE_NORMAL);
```

**Suggested fix:** add an overload that accepts an explicit oscillator frequency:

```cpp
CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, uint32_t fsysHz);
```

---

## Issue 3 — Designated initialisers don't compile under Arduino's C++ dialect

`CanMsg` has no constructor, so the natural way to initialise it is with designated initialisers:

```cpp
CanMsg tx = { .id = 0x123, .fdf = true, .brs = true, .dlc = 8 };
```

This fails to compile with the Arduino ESP32 framework (GCC 8.4, `-std=gnu++11`) because designated initialisers for non-aggregate types are a C++20 feature.

**Workaround:** assign fields individually after declaration:

```cpp
CanMsg tx;
tx.id  = 0x123;
tx.fdf = true;
tx.brs = true;
tx.dlc = 8;
```

**Suggested fix:** add a convenience constructor to `CanMsg`:

```cpp
CanMsg(uint32_t id, bool fdf, bool brs, uint8_t dlc);
```

Or document that designated initialisers require C++20 and that Arduino users must assign fields individually.
