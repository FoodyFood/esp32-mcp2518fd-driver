#pragma once

#include <stdint.h>
#include <stdbool.h>

// ----------------------------------------------------------------------------
// Pure bit-timing calculations — no hardware dependency.
// Extracted here so they can be unit-tested on native without Arduino.
//
// calcBitTiming()
//   Computes NBTCFG, DBTCFG and TDCFG register words for the given rates and
//   oscillator frequency.  BRP=0 throughout; 80% sample point (75% for 5 Mbps
//   where TSEG1 would otherwise exceed the 5-bit field).
//   Returns false if the rate is not exactly achievable.
//   Formula: rate = fsys / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-1/3-4]
//
// calcTxTimeout()
//   Returns a TX-done poll timeout in milliseconds for a worst-case 64-byte
//   CAN FD frame at the given nominal and data bit timings.

inline bool calcBitTiming(uint32_t fsys, uint32_t nominalBps, uint32_t dataBps,
                           uint32_t& nbtcfg, uint32_t& dbtcfg, uint32_t& tdcfg)
{
    // --- Nominal ---
    // BRP=0, SP=80%: TSEG2 = totalTQ/5, TSEG1 = totalTQ - 1 - TSEG2
    // TSEG1 field: 8-bit (max 255), TSEG2 field: 7-bit (max 127)
    {
        if (nominalBps == 0) return false;
        uint32_t totalTQ = fsys / nominalBps;
        if (totalTQ < 3 || (fsys % nominalBps) != 0) return false;

        uint32_t tseg2 = totalTQ / 5;
        if (tseg2 < 1)   tseg2 = 1;
        if (tseg2 > 127) tseg2 = 127;
        uint32_t tseg1 = totalTQ - 1 - tseg2;
        if (tseg1 > 255 || tseg1 < 1) return false;

        uint32_t sjw = tseg2;
        nbtcfg = (0u << 24) | (tseg1 << 16) | (tseg2 << 8) | sjw;
    }

    // --- Data ---
    // BRP=0, SP=80%: TSEG2 = totalTQ/5, TSEG1 = totalTQ - 1 - TSEG2
    // TSEG1 field: 5-bit (max 31), TSEG2 field: 4-bit (max 15)
    {
        if (dataBps == 0) return false;
        uint32_t totalTQ = fsys / dataBps;
        if (totalTQ < 3 || (fsys % dataBps) != 0) return false;

        uint32_t tseg2 = totalTQ / 5;
        if (tseg2 < 1)  tseg2 = 1;
        if (tseg2 > 15) tseg2 = 15;
        uint32_t tseg1 = totalTQ - 1 - tseg2;
        if (tseg1 > 31) tseg1 = 31;  // cap to field width, recheck rate below
        if (tseg1 < 1)  return false;

        // Verify capped values still produce the exact target rate
        if (fsys / (1 + tseg1 + tseg2) != dataBps) return false;

        uint32_t sjw = tseg2;
        dbtcfg = (0u << 24) | (tseg1 << 16) | (tseg2 << 8) | sjw;

        // TDC: required at >= 1 Mbps. TDCO = (BRP+1)*(TSEG1+1) [ref manual Table 3-5]
        if (dataBps >= 1000000u)
        {
            uint32_t tdco = tseg1 + 1;  // BRP=0 so (BRP+1)=1
            tdcfg = (2u << 16) | (tdco << 8);  // TDCMOD=auto(2), TDCO
        }
        else
        {
            tdcfg = 0;
        }
    }

    return true;
}

inline uint32_t calcTxTimeout(uint32_t fsys, uint32_t nbtcfg, uint32_t dbtcfg)
{
    uint32_t nBrp   = (nbtcfg >> 24) & 0xFF;
    uint32_t nTseg1 = (nbtcfg >> 16) & 0xFF;
    uint32_t nTseg2 = (nbtcfg >>  8) & 0x7F;
    uint32_t dBrp   = (dbtcfg >> 24) & 0xFF;
    uint32_t dTseg1 = (dbtcfg >> 16) & 0x1F;
    uint32_t dTseg2 = (dbtcfg >>  8) & 0x0F;

    if (fsys == 0) fsys = 20000000u;
    uint32_t tq_ns = 1000000000u / fsys;

    uint32_t nbt_ns = (nBrp + 1) * (1 + nTseg1 + nTseg2) * tq_ns;
    uint32_t dbt_ns = (dBrp + 1) * (1 + dTseg1 + dTseg2) * tq_ns;

    // Worst-case 64-byte CAN FD frame: ~36 nominal bits + ~562 data bits
    uint32_t frame_ns = 36 * nbt_ns + 562 * dbt_ns;
    uint32_t ms = (frame_ns * 3 / 1000000) + 2;
    return ms < 2 ? 2 : ms;
}

// EID T0 word encode: SID[10:0]=id>>18, EID[17:0]=id&0x3FFFF packed into T0
// (DS20006027B Table 3-5)
inline uint32_t encodeEidT0(uint32_t id29)
{
    uint32_t sid = (id29 >> 18) & 0x7FFu;
    uint32_t eid = id29 & 0x3FFFFu;
    return sid | (eid << 11);
}

// EID T0 word decode: reconstruct 29-bit ID from T0
inline uint32_t decodeEidT0(uint32_t t0)
{
    uint32_t sid = t0 & 0x7FFu;
    uint32_t eid = (t0 >> 11) & 0x3FFFFu;
    return (sid << 18) | eid;
}

// Filter OBJ encode for EID: same layout as T0, EXIDE at bit 30
inline uint32_t encodeFilterObjEid(uint32_t id29)
{
    return encodeEidT0(id29) | (1u << 30);
}

// Filter MASK encode for EID: same layout, MIDE at bit 30
inline uint32_t encodeFilterMskEid(uint32_t mask29)
{
    uint32_t msid = (mask29 >> 18) & 0x7FFu;
    uint32_t meid = mask29 & 0x3FFFFu;
    return msid | (meid << 11) | (1u << 30);
}
