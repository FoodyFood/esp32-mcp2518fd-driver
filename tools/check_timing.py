"""Verify bit timing presets against the datasheet formula."""

FSYS = 40e6  # 40 MHz

presets = {
    "NBTCFG_125K": 0x001E0707,
    "NBTCFG_250K": 0x000E0303,
    "NBTCFG_500K": 0x00060303,
    "NBTCFG_1M":   0x00060101,
    "DBTCFG_1M":   0x00260101,
    "DBTCFG_2M":   0x00120101,
    "DBTCFG_4M":   0x00070202,
    "DBTCFG_5M":   0x00050202,
    "DBTCFG_8M":   0x00030101,
}

for name, val in presets.items():
    brp   = (val >> 24) & 0xFF
    tseg1 = (val >> 16) & 0xFF
    tseg2 = (val >>  8) & 0x7F
    sjw   =  val        & 0x7F
    tq_ns = (brp + 1) * 1e9 / FSYS
    nbt_ns = (1 + tseg1 + tseg2) * tq_ns
    rate_kbps = 1e6 / nbt_ns
    print(f"{name:20s}  BRP={brp:3d} TSEG1={tseg1:3d} TSEG2={tseg2:2d}  TQ={tq_ns:.1f}ns  NBT={nbt_ns:.1f}ns  = {rate_kbps:.1f} kbps")
