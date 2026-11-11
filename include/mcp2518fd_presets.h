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
// Formula: rate = FSYS / ((BRP+1) * (1 + TSEG1 + 1 + TSEG2 + 1))  [ref manual Eq 3-1/3-4]
// All presets use BRP=0, exact rate, mcp2518fd_lite algorithm (~80% sample point).
// Verified against calcBitTiming().

// 40 MHz — Nominal
constexpr uint32_t NBTCFG_125K_40MHZ = 0x00FE3F3F;  // 125 kbps  BRP=0 TSEG1=254 TSEG2=63  SJW=63  SP=80.0%
constexpr uint32_t NBTCFG_250K_40MHZ = 0x007E1F1F;  // 250 kbps  BRP=0 TSEG1=126 TSEG2=31  SJW=31  SP=80.0%
constexpr uint32_t NBTCFG_500K_40MHZ = 0x003E0F0F;  // 500 kbps  BRP=0 TSEG1=62  TSEG2=15  SJW=15  SP=80.0%
constexpr uint32_t NBTCFG_1M_40MHZ   = 0x001E0707;  // 1 Mbps    BRP=0 TSEG1=30  TSEG2=7   SJW=7   SP=80.0%
// 40 MHz — Data
constexpr uint32_t DBTCFG_1M_40MHZ   = 0x001E0707;  // 1 Mbps    BRP=0 TSEG1=30 TSEG2=7  SJW=7  SP=80.0%
constexpr uint32_t DBTCFG_2M_40MHZ   = 0x000E0303;  // 2 Mbps    BRP=0 TSEG1=14 TSEG2=3  SJW=3  SP=80.0%
constexpr uint32_t DBTCFG_4M_40MHZ   = 0x00060101;  // 4 Mbps    BRP=0 TSEG1=6  TSEG2=1  SJW=1  SP=80.0%
constexpr uint32_t DBTCFG_5M_40MHZ   = 0x00040101;  // 5 Mbps    BRP=0 TSEG1=4  TSEG2=1  SJW=1  SP=75.0%
constexpr uint32_t DBTCFG_8M_40MHZ   = 0x00010101;  // 8 Mbps    BRP=0 TSEG1=1  TSEG2=1  SJW=1  SP=60.0%
// 40 MHz — TDC  (TDCO = TSEG1+1, TDCMOD=auto)
constexpr uint32_t TDC_DISABLED   = 0x00000000;
constexpr uint32_t TDC_1M_40MHZ   = 0x00021F00;  // TDCO=31
constexpr uint32_t TDC_2M_40MHZ   = 0x00020F00;  // TDCO=15
constexpr uint32_t TDC_4M_40MHZ   = 0x00020700;  // TDCO=7
constexpr uint32_t TDC_5M_40MHZ   = 0x00020500;  // TDCO=5
constexpr uint32_t TDC_8M_40MHZ   = 0x00020200;  // TDCO=2
// 20 MHz — Nominal
constexpr uint32_t NBTCFG_125K_20MHZ = 0x007E1F1F;  // 125 kbps  BRP=0 TSEG1=126 TSEG2=31  SJW=31  SP=80.0%
constexpr uint32_t NBTCFG_250K_20MHZ = 0x003E0F0F;  // 250 kbps  BRP=0 TSEG1=62  TSEG2=15  SJW=15  SP=80.0%
constexpr uint32_t NBTCFG_500K_20MHZ = 0x001E0707;  // 500 kbps  BRP=0 TSEG1=30  TSEG2=7   SJW=7   SP=80.0%
constexpr uint32_t NBTCFG_1M_20MHZ   = 0x000E0303;  // 1 Mbps    BRP=0 TSEG1=14  TSEG2=3   SJW=3   SP=80.0%
// 20 MHz — Data
constexpr uint32_t DBTCFG_1M_20MHZ   = 0x000E0303;  // 1 Mbps    BRP=0 TSEG1=14 TSEG2=3  SJW=3  SP=80.0%
constexpr uint32_t DBTCFG_2M_20MHZ   = 0x00060101;  // 2 Mbps    BRP=0 TSEG1=6  TSEG2=1  SJW=1  SP=80.0%
constexpr uint32_t DBTCFG_4M_20MHZ   = 0x00010101;  // 4 Mbps    BRP=0 TSEG1=1  TSEG2=1  SJW=1  SP=60.0%
// Note: 5 Mbps at 20 MHz requires only 4 TQ which is below minimum — use 40 MHz oscillator
// 20 MHz — TDC
constexpr uint32_t TDC_1M_20MHZ   = 0x00020F00;  // TDCO=15
constexpr uint32_t TDC_2M_20MHZ   = 0x00020700;  // TDCO=7
constexpr uint32_t TDC_4M_20MHZ   = 0x00020200;  // TDCO=2
