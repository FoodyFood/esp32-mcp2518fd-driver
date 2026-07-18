"""Verify bit timing presets against the datasheet formula."""

def check(name, val, fsys):
    brp   = (val >> 24) & 0xFF
    tseg1 = (val >> 16) & 0xFF
    tseg2 = (val >>  8) & 0x7F
    sjw   =  val        & 0x7F
    tq_ns = (brp + 1) * 1e9 / fsys
    nbt_ns = (1 + tseg1 + tseg2) * tq_ns
    rate_kbps = 1e6 / nbt_ns
    sp = 100 * (1 + tseg1) / (1 + tseg1 + tseg2)
    print(f"{name:24s}  BRP={brp:3d} TSEG1={tseg1:3d} TSEG2={tseg2:2d} SJW={sjw:2d}  "
          f"TQ={tq_ns:.1f}ns  NBT={nbt_ns:.1f}ns  = {rate_kbps:.1f} kbps  SP={sp:.0f}%")

presets_40 = {
    "NBTCFG_125K_40MHZ": 0x00FF4040,
    "NBTCFG_250K_40MHZ": 0x007F2020,
    "NBTCFG_500K_40MHZ": 0x003F1010,
    "NBTCFG_1M_40MHZ":   0x001F0808,
    "DBTCFG_1M_40MHZ":   0x001F0808,
    "DBTCFG_2M_40MHZ":   0x000F0404,
    "DBTCFG_4M_40MHZ":   0x00070202,
    "DBTCFG_5M_40MHZ":   0x00050202,
    "DBTCFG_8M_40MHZ":   0x00030101,
}

presets_20 = {
    "NBTCFG_125K_20MHZ": 0x007F2020,
    "NBTCFG_250K_20MHZ": 0x003F1010,
    "NBTCFG_500K_20MHZ": 0x001F0808,
    "NBTCFG_1M_20MHZ":   0x000F0404,
    "DBTCFG_1M_20MHZ":   0x000F0404,
    "DBTCFG_2M_20MHZ":   0x00070202,
    "DBTCFG_4M_20MHZ":   0x00030101,
    "DBTCFG_5M_20MHZ":   0x00020101,
    "DBTCFG_8M_20MHZ":   0x00010101,
}

print("--- 40 MHz ---")
for name, val in presets_40.items():
    check(name, val, 40e6)

print("--- 20 MHz ---")
for name, val in presets_20.items():
    check(name, val, 20e6)

