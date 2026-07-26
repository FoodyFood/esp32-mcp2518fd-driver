# SPEC-007 — Public API Review and Cleanup

## Goal

Resolve the API issues identified in the pre-SPEC-008 audit before new features are added.
The driver is approaching a stable public surface. Every new feature (SPEC-008 CLKO,
SPEC-009 dual INT, SPEC-010 classic mode) will add parameters or state — fixing the
structural issues now prevents them compounding.

No new hardware features. No register changes. Pure API surface work.

---

## Issues and decisions

### Issue 1 — configure() parameter list is growing (Medium)

`configure(nominalBps, dataBps, mode, rxFifoDepth, enableTimestamp)` already has 5
parameters. SPEC-008 (classic mode), SPEC-009 (CLKO), SPEC-010 (dual INT) will each
want to add more. Positional parameters do not scale.

**Decision:** Introduce a `CanConfig` struct with safe defaults. The simple overload
stays identical to today:

```cpp
can.configure(500000, 2000000, MODE_NORMAL);  // unchanged
```

Advanced callers name only what they need:

```cpp
CanConfig cfg;
cfg.rxFifoDepth     = 24;
cfg.enableTimestamp = true;
can.configure(500000, 2000000, MODE_NORMAL, cfg);
```

`configureRaw()` adopts the same struct so both paths share one set of options.

---

### Issue 2 — stop()/restart() and sleep()/wake() share mPrevMode (Medium)

Both `stop()` and `sleep()` write `mPrevMode`. Calling `stop()` then `sleep()` then
`wake()` restores `MODE_CONFIG` instead of the original running mode — a silent
correctness bug.

**Decision:** Give each pair its own saved-mode field:
- `mStopPrevMode` — written by `stop()`, read by `restart()`
- `mSleepPrevMode` — written by `sleep()`, read by `wake()`

No API change. Internal fix only.

---

### Issue 3 — getErrors() clears rxOverflow as a side effect (Medium)

Calling `getErrors()` twice returns `rxOverflow=true` then `rxOverflow=false`. The
clearing is invisible from the signature.

**Decision:** Rename to `readAndClearErrors()`. The name makes the side effect
explicit. `hasErrors()` stays unchanged as the cheap polling alternative.

---

### Issue 4 — receive() has a redundant non-blocking overload (Low)

`receive(msg)` and `receive(msg, 0)` are identical. Two overloads for one behaviour
adds surface area without capability.

**Decision:** Collapse to a single overload with a default:

```cpp
bool receive(CanMsg& msg, uint32_t timeoutMs = 0);
```

Existing callers of `receive(rx)` and `receive(rx, 500)` both compile unchanged.

---

### Issue 5 — setFilter() catch-all is a hidden idiom (Low)

`setFilter(0, 0, 0, false)` is the catch-all but nothing in the API names it.

**Decision:** Add `void resetFilters()` — clears filters 1–31 and reinstalls the
catch-all on filter 0. This is what `configure()` already does internally; expose it
so callers can return to passthrough after applying selective filters.

---

### Issue 6 — CanMsg.brs is silently ignored when fdf=false (Low)

Setting `brs=true` on a classic frame is a no-op. No validation, no comment.

**Decision:** Add a comment on the `brs` field: "only meaningful when fdf=true".
No runtime guard — the chip handles it correctly; the comment prevents confusion.

---

### Issue 7 — readOsc() leaks the SPI layer into the public API (Low)

`readOsc()` returns a raw register word. `getFsys()` already exposes the useful
derived value.

**Decision:** Remove `readOsc()` from the public API. It is used in no example and
no test harness. If a future diagnostic need arises, it can be re-added then.

---

### Issue 8 — dlcToLen() is exposed in the public header unnecessarily (Low)

`dlcToLen()` is a pure logic function used internally. It is already in
`mcp2518fd_can.h` but belongs in `mcp2518fd_timing.h` alongside the other pure
logic helpers, where it is already unit-tested.

**Decision:** Move `dlcToLen()` to `mcp2518fd_timing.h`. Remove it from
`mcp2518fd_can.h`. All internal call sites already include `mcp2518fd_timing.h`.

---

### Issue 9 — intPin sentinel -1 is a magic number (Low)

`MCP2518Driver(spi, cs, -1)` is not self-documenting.

**Decision:** Add `constexpr int8_t NO_INT_PIN = -1;` and use it as the default
parameter value. Existing callers that omit the third argument are unaffected.
Callers that pass `-1` explicitly can optionally update to `NO_INT_PIN`.

---

## Acceptance criteria

- [ ] `CanConfig` struct exists with `rxFifoDepth=16` and `enableTimestamp=false` defaults
- [ ] `configure(nominalBps, dataBps, mode)` compiles and behaves identically to today
- [ ] `configure(nominalBps, dataBps, mode, cfg)` accepts a `CanConfig` and applies it
- [ ] `configureRaw(nbtcfg, dbtcfg, tdcfg, mode)` and `configureRaw(nbtcfg, dbtcfg, tdcfg, mode, cfg)` both compile
- [ ] `stop()` then `sleep()` then `wake()` restores the original running mode, not `MODE_CONFIG`
- [ ] `getErrors()` is renamed `readAndClearErrors()` — all call sites updated
- [ ] `receive(msg)` and `receive(msg, 500)` both compile via single overload with default
- [ ] `resetFilters()` is public and reinstalls the catch-all
- [ ] `readOsc()` is removed from the public header
- [ ] `dlcToLen()` is in `mcp2518fd_timing.h` only, not in `mcp2518fd_can.h`
- [ ] `NO_INT_PIN` constant is defined and used as the default for `intPin`
- [ ] All examples compile without modification
- [ ] All integration test harnesses compile without modification
- [ ] Unit tests pass (88 succeeded)
- [ ] single_node, id_filter and two_node suites all report PASS

---

## Files affected

- `include/mcp2518fd_can.h` — CanConfig struct, NO_INT_PIN, remove readOsc(), remove dlcToLen(), rename getErrors(), collapse receive() overloads, add resetFilters()
- `src/mcp2518fd_can.cpp` — mStopPrevMode / mSleepPrevMode split, resetFilters() impl, rename getErrors()
- `include/mcp2518fd_timing.h` — add dlcToLen() (move from can.h)
- `examples/` — update any `getErrors()` call sites to `readAndClearErrors()`
- `tests/integration/` — update any `getErrors()` call sites to `readAndClearErrors()`

---

## Out of scope

- No new hardware features
- No register changes
- No timing changes
- No new examples (the existing examples already cover the affected API surface)
