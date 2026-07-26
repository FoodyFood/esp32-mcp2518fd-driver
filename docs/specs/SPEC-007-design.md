# SPEC-007 — Design

## Approach

Nine targeted changes to `mcp2518fd_can.h`, `mcp2518fd_can.cpp` and `mcp2518fd_timing.h`.
No register changes. No new hardware behaviour. All changes are additive or renames —
existing callers either compile unchanged or require a mechanical find-and-replace.

Changes are ordered so each task leaves the codebase in a buildable state.

---

## Tasks

### Task 1 — Move `dlcToLen()` to `mcp2518fd_timing.h`

`dlcToLen()` is a pure logic function. It belongs alongside `calcBitTiming()` and
`calcTxTimeout()` in `mcp2518fd_timing.h`, not in the public driver header.

**Changes:**
- `mcp2518fd_timing.h` — add `dlcToLen()` (copy from `mcp2518fd_can.h`)
- `mcp2518fd_can.h` — remove `dlcToLen()`

`mcp2518fd_can.cpp` already includes `mcp2518fd_timing.h` so all internal call sites
are covered. `two_node` harness calls `dlcToLen()` directly and also includes
`mcp2518fd_can.h` which includes `mcp2518fd_timing.h` — no change needed there.

**Verify:** unit tests still pass (`dlcToLen` tests are already in `test_main.cpp`).

---

### Task 2 — Add `NO_INT_PIN` constant

Replace the magic `-1` sentinel with a named constant.

**Changes:**
- `mcp2518fd_can.h` — add before the class declaration:
  ```cpp
  constexpr int8_t NO_INT_PIN = -1;
  ```
- `mcp2518fd_can.h` — update constructor default: `int8_t intPin = NO_INT_PIN`
- `mcp2518fd_can.cpp` — update constructor initialiser comment if present

No call sites need updating — callers that omit the third argument are unaffected.

---

### Task 3 — Add `CanConfig` struct and update `configure()` / `configureRaw()`

Introduce a config struct so optional parameters are named, not positional.

**Changes to `mcp2518fd_can.h`:**
- Add struct before the class:
  ```cpp
  struct CanConfig
  {
      uint8_t rxFifoDepth     = 16;
      bool    enableTimestamp = false;
  };
  ```
- Change `configure()` signature:
  ```cpp
  // Simple form — unchanged call sites compile as-is
  CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode);

  // Advanced form — named options only
  CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, const CanConfig& cfg);
  ```
- Change `configureRaw()` signature the same way:
  ```cpp
  CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode);
  CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode, const CanConfig& cfg);
  ```

**Changes to `mcp2518fd_can.cpp`:**
- Simple overloads delegate to the cfg overload with a default-constructed `CanConfig`:
  ```cpp
  CanStatus MCP2518Driver::configure(uint32_t n, uint32_t d, uint8_t mode)
  {
      return configure(n, d, mode, CanConfig{});
  }
  ```
- The cfg overload contains the existing implementation body, replacing the
  `rxFifoDepth` and `enableTimestamp` parameters with `cfg.rxFifoDepth` and
  `cfg.enableTimestamp`.
- Same pattern for `configureRaw()`.

**Call sites to update** — existing callers that pass `rxFifoDepth` or
`enableTimestamp` as positional arguments must be updated to use `CanConfig`:

| File | Current call | Updated call |
|---|---|---|
| `single_node/src/main.cpp` | `configure(125000, 2000000, MODE_INTERNAL_LB, 16)` | `configure(125000, 2000000, MODE_INTERNAL_LB, {.rxFifoDepth=16})` |
| `single_node/src/main.cpp` | `configure(125000, 2000000, MODE_INTERNAL_LB, 4)` | `configure(125000, 2000000, MODE_INTERNAL_LB, {.rxFifoDepth=4})` |
| `single_node/src/main.cpp` | `configure(125000, 2000000, MODE_INTERNAL_LB, 16, true)` | `configure(125000, 2000000, MODE_INTERNAL_LB, {.rxFifoDepth=16, .enableTimestamp=true})` |
| `two_node/src/main.cpp` | `configure(125000, 2000000, MODE_NORMAL, 4)` | `configure(125000, 2000000, MODE_NORMAL, {.rxFifoDepth=4})` |
| `README.md` | `can.configure(500000, 2000000, MODE_NORMAL, 16, true)` | `CanConfig cfg{.rxFifoDepth=16, .enableTimestamp=true};` then `can.configure(500000, 2000000, MODE_NORMAL, cfg);` |

All other `configure()` calls use only the 3-argument form and need no change.

---

### Task 4 — Collapse `receive()` to a single overload with default

**Changes to `mcp2518fd_can.h`:**
- Remove: `bool receive(CanMsg& msg);`
- Change: `bool receive(CanMsg& msg, uint32_t timeoutMs);`
  to: `bool receive(CanMsg& msg, uint32_t timeoutMs = 0);`

**Changes to `mcp2518fd_can.cpp`:**
- Remove the `receive(CanMsg& msg)` implementation body (the one that calls
  `receive(msg, 0)`). The single remaining implementation already handles `timeoutMs=0`
  correctly — the while loop exits immediately when `timeoutMs` is 0 and `available()`
  is false.

**Call sites:** all existing `can.receive(rx)` calls compile unchanged via the default.
All existing `can.receive(rx, 500)` calls compile unchanged. No edits needed.

---

### Task 5 — Fix `mPrevMode` collision between `stop()`/`restart()` and `sleep()`/`wake()`

**Changes to `mcp2518fd_can.h` (private section):**
- Replace `uint8_t mPrevMode` with:
  ```cpp
  uint8_t mStopPrevMode  = MODE_CONFIG;
  uint8_t mSleepPrevMode = MODE_CONFIG;
  ```

**Changes to `mcp2518fd_can.cpp`:**
- `stop()` — write `mStopPrevMode = mSpi.getMode()`
- `restart()` — read `mStopPrevMode`
- `sleep()` — write `mSleepPrevMode = mSpi.getMode()`
- `wake()` — read `mSleepPrevMode`

No API change. No call site updates needed.

---

### Task 6 — Rename `getErrors()` to `readAndClearErrors()`

**Changes to `mcp2518fd_can.h`:**
- Rename declaration: `CanError readAndClearErrors();`
- Remove old `CanError getErrors();`

**Changes to `mcp2518fd_can.cpp`:**
- Rename implementation.

**Call sites to update:**

| File | Occurrences |
|---|---|
| `tests/integration/single_node/src/main.cpp` | 5 — lines ~175, 185, 200, 215, 240 |
| `tests/integration/two_node/src/main.cpp` | 3 — lines ~175, 195, 220 |

Search token: `can.getErrors()` → replace with `can.readAndClearErrors()`.
No example files use `getErrors()` — confirmed by inspection.

---

### Task 7 — Remove `readOsc()` from the public API

**Changes to `mcp2518fd_can.h`:**
- Remove `uint32_t readOsc();` declaration.

**Changes to `mcp2518fd_can.cpp`:**
- Remove `uint32_t MCP2518Driver::readOsc()` implementation.

**Call sites:** `readOsc()` is not called in any example or test harness — confirmed
by inspection. No other files need updating.

---

### Task 8 — Add `resetFilters()`

Expose the internal catch-all restore as a public method.

**Changes to `mcp2518fd_can.h`:**
- Add to public section:
  ```cpp
  // Restore the catch-all filter (accept all frames) and disable filters 1–31.
  // Call this to return to passthrough mode after applying selective filters.
  void resetFilters();
  ```

**Changes to `mcp2518fd_can.cpp`:**
- Implement by calling the existing private `configFilter()` body directly,
  then clearing filters 1–31:
  ```cpp
  void MCP2518Driver::resetFilters()
  {
      configFilter();  // reinstalls catch-all on filter 0
      for (uint8_t i = 1; i <= 31; i++) clearFilter(i);
  }
  ```

No call site updates needed — this is a new addition.

---

### Task 9 — Document `CanMsg.brs` constraint

**Changes to `mcp2518fd_can.h`:**
- Update the `brs` field comment:
  ```cpp
  bool brs = false; // true = switch to data bit rate in payload phase (only meaningful when fdf=true)
  ```

No code change. No call site updates.

---

## Build and test sequence

After all tasks are complete:

```
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
```
Must report 88 succeeded. Then generate the coverage report:

```
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && python3 coverage.py"
```
Must report 100% lines, 100% functions.

```
python tests/integration/verify.py --suite single_node --port COM4
python tests/integration/verify.py --suite id_filter --port COM4
python tests/integration/verify.py --suite two_node --port COM4 --port-b COM3
```
All three must report PASS before committing.

---

## Commit plan

All nine tasks in a single commit — they are all mechanical, no behaviour change,
and splitting them would leave the codebase in a partially-renamed state.

```
git add . && git commit -m "SPEC-007 step 1: API review cleanup"
```
