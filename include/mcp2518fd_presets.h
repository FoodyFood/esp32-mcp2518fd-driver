#pragma once

#ifdef ARDUINO
#  include <Arduino.h>
#else
#  include <stdint.h>
#endif

// ----------------------------------------------------------------------------
// Bit timing presets — advanced use only
//
// Pre-computed register words for common rates and oscillator frequencies.
// Prefer configure(nominalBps, dataBps, mode) which selects values automatically.
//
// Formula: rate = FSYS / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-1/3-4]
// All presets use BRP=0, exact rate, 80% sample point (75% for 5 Mbps).
// Verified against tools/check_timing.py.

// 40 MHz — Nominal
constexpr uint32_t NBTCFG_125K_40MHZ = 0x00FF4040;  // 125 kbps  BRP=0 TSEG1=255 TSEG2=64  SJW=64  SP=80%
constexpr uint32_t NBTCFG_250K_40MHZ = 0x007F2020;  // 250 kbps  BRP=0 TSEG1=127 TSEG2=32  SJW=32  SP=80%
constexpr uint32_t NBTCFG_500K_40MHZ = 0x003F1010;  // 500 kbps  BRP=0 TSEG1=63  TSEG2=16  SJW=16  SP=80%
constexpr uint32_t NBTCFG_1M_40MHZ   = 0x001F0808;  // 1 Mbps    BRP=0 TSEG1=31  TSEG2=8   SJW=8   SP=80%
// 40 MHz — Data
constexpr uint32_t DBTCFG_1M_40MHZ   = 0x001F0808;  // 1 Mbps    BRP=0 TSEG1=31 TSEG2=8  SJW=8  SP=80%
constexpr uint32_t DBTCFG_2M_40MHZ   = 0x000F0404;  // 2 Mbps    BRP=0 TSEG1=15 TSEG2=4  SJW=4  SP=80%
constexpr uint32_t DBTCFG_4M_40MHZ   = 0x00070202;  // 4 Mbps    BRP=0 TSEG1=7  TSEG2=2  SJW=2  SP=80%
constexpr uint32_t DBTCFG_5M_40MHZ   = 0x00050202;  // 5 Mbps    BRP=0 TSEG1=5  TSEG2=2  SJW=2  SP=75%
constexpr uint32_t DBTCFG_8M_40MHZ   = 0x00030101;  // 8 Mbps    BRP=0 TSEG1=3  TSEG2=1  SJW=1  SP=80%
// 40 MHz — TDC  (TDCO = (BRP+1)*(TSEG1+1), TDCMOD=auto)
constexpr uint32_t TDC_DISABLED   = 0x00000000;
constexpr uint32_t TDC_1M_40MHZ   = 0x00022000;  // TDCO=32
constexpr uint32_t TDC_2M_40MHZ   = 0x00021000;  // TDCO=16
constexpr uint32_t TDC_4M_40MHZ   = 0x00020800;  // TDCO=8
constexpr uint32_t TDC_5M_40MHZ   = 0x00020600;  // TDCO=6
constexpr uint32_t TDC_8M_40MHZ   = 0x00020400;  // TDCO=4
// 20 MHz — Nominal
constexpr uint32_t NBTCFG_125K_20MHZ = 0x007F2020;  // 125 kbps  BRP=0 TSEG1=127 TSEG2=32  SJW=32  SP=80%
constexpr uint32_t NBTCFG_250K_20MHZ = 0x003F1010;  // 250 kbps  BRP=0 TSEG1=63  TSEG2=16  SJW=16  SP=80%
constexpr uint32_t NBTCFG_500K_20MHZ = 0x001F0808;  // 500 kbps  BRP=0 TSEG1=31  TSEG2=8   SJW=8   SP=80%
constexpr uint32_t NBTCFG_1M_20MHZ   = 0x000F0404;  // 1 Mbps    BRP=0 TSEG1=15  TSEG2=4   SJW=4   SP=80%
// 20 MHz — Data
constexpr uint32_t DBTCFG_1M_20MHZ   = 0x000F0404;  // 1 Mbps    BRP=0 TSEG1=15 TSEG2=4  SJW=4  SP=80%
constexpr uint32_t DBTCFG_2M_20MHZ   = 0x00070202;  // 2 Mbps    BRP=0 TSEG1=7  TSEG2=2  SJW=2  SP=80%
constexpr uint32_t DBTCFG_4M_20MHZ   = 0x00030101;  // 4 Mbps    BRP=0 TSEG1=3  TSEG2=1  SJW=1  SP=80%
constexpr uint32_t DBTCFG_5M_20MHZ   = 0x00020101;  // 5 Mbps    BRP=0 TSEG1=2  TSEG2=1  SJW=1  SP=75%
// 20 MHz — TDC
constexpr uint32_t TDC_1M_20MHZ   = 0x00021000;  // TDCO=16
constexpr uint32_t TDC_2M_20MHZ   = 0x00020800;  // TDCO=8
constexpr uint32_t TDC_4M_20MHZ   = 0x00020400;  // TDCO=4
constexpr uint32_t TDC_5M_20MHZ   = 0x00020300;  // TDCO=3
