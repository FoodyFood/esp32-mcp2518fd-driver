"""Find correct NBTCFG values for nominal rates at 40 MHz."""

FSYS = 40e6
targets = {125e3: "125K", 250e3: "250K", 500e3: "500K", 1e6: "1M"}

for target, label in targets.items():
    target_nbt_ns = 1e9 / target
    print(f"\n--- {label} ({target/1000:.0f} kbps, NBT={target_nbt_ns:.0f}ns) ---")
    found = []
    for brp in range(256):
        tq_ns = (brp + 1) * 25.0
        ntq = target_nbt_ns / tq_ns
        if ntq == int(ntq) and 4 <= ntq <= 385:
            ntq = int(ntq)
            # Try a few TSEG1/TSEG2 splits with ~75-80% sample point
            for tseg2 in range(1, min(128, ntq)):
                tseg1 = ntq - 1 - tseg2
                if tseg1 < 1 or tseg1 > 255: continue
                sp = (1 + tseg1) / ntq * 100
                if 75 <= sp <= 80:
                    sjw = min(tseg2, 16)
                    reg = (brp << 24) | (tseg1 << 16) | (tseg2 << 8) | sjw
                    found.append((brp, tseg1, tseg2, sjw, sp, reg))
    for f in found[:3]:
        brp, ts1, ts2, sjw, sp, reg = f
        print(f"  BRP={brp} TSEG1={ts1} TSEG2={ts2} SJW={sjw}  SP={sp:.1f}%  reg=0x{reg:08X}")
