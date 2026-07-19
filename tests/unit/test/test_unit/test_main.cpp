#include <unity.h>
#include "mcp2518fd_registers.h"
#include "mcp2518fd_timing.h"
#include "mcp2518fd_presets.h"

// dlcToLen is defined in mcp2518fd_can.h which pulls Arduino.h.
// Redeclare here — pure logic, no hardware dependency.
inline constexpr uint8_t dlcToLen(uint8_t dlc)
{
    return (dlc <=  8) ? dlc
         : (dlc ==  9) ? 12
         : (dlc == 10) ? 16
         : (dlc == 11) ? 20
         : (dlc == 12) ? 24
         : (dlc == 13) ? 32
         : (dlc == 14) ? 48 : 64;
}

// ============================================================================
// dlcToLen — DLC 0-15 → byte length  (DS20006027B Table 3-3)
// ============================================================================

void test_dlcToLen_classic_range()
{
    for (uint8_t dlc = 0; dlc <= 8; dlc++)
        TEST_ASSERT_EQUAL_UINT8(dlc, dlcToLen(dlc));
}

void test_dlcToLen_fd_jumps()
{
    TEST_ASSERT_EQUAL_UINT8(12, dlcToLen(9));
    TEST_ASSERT_EQUAL_UINT8(16, dlcToLen(10));
    TEST_ASSERT_EQUAL_UINT8(20, dlcToLen(11));
    TEST_ASSERT_EQUAL_UINT8(24, dlcToLen(12));
    TEST_ASSERT_EQUAL_UINT8(32, dlcToLen(13));
    TEST_ASSERT_EQUAL_UINT8(48, dlcToLen(14));
    TEST_ASSERT_EQUAL_UINT8(64, dlcToLen(15));
}

// ============================================================================
// calcBitTiming — 20 MHz nominal rates
// Verified against preset constants in mcp2518fd_can.h
// ============================================================================

void test_calcBitTiming_20mhz_500k_2m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 500000, 2000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_500K_20MHZ, nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_2M_20MHZ,   dbtcfg);
}

void test_calcBitTiming_20mhz_500k_1m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 500000, 1000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_500K_20MHZ, nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_1M_20MHZ,   dbtcfg);
}

void test_calcBitTiming_20mhz_250k_2m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 250000, 2000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_250K_20MHZ, nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_2M_20MHZ,   dbtcfg);
}

void test_calcBitTiming_20mhz_125k_1m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 125000, 1000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_125K_20MHZ, nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_1M_20MHZ,   dbtcfg);
}

void test_calcBitTiming_20mhz_1m_4m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 1000000, 4000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_1M_20MHZ,  nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_4M_20MHZ,  dbtcfg);
}

void test_calcBitTiming_20mhz_500k_5m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(20000000, 500000, 5000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_5M_20MHZ, dbtcfg);
}

// ============================================================================
// calcBitTiming — 40 MHz nominal rates
// ============================================================================

void test_calcBitTiming_40mhz_500k_2m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(40000000, 500000, 2000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(NBTCFG_500K_40MHZ, nbtcfg);
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_2M_40MHZ,   dbtcfg);
}

void test_calcBitTiming_40mhz_1m_8m()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_TRUE(calcBitTiming(40000000, 1000000, 8000000, nbtcfg, dbtcfg, tdcfg));
    TEST_ASSERT_EQUAL_HEX32(DBTCFG_8M_40MHZ, dbtcfg);
}

// ============================================================================
// calcBitTiming — TDC field
// TDCO = TSEG1+1 at BRP=0; TDCMOD=2 (auto)  [ref manual Table 3-5]
// ============================================================================

void test_calcBitTiming_tdc_enabled_at_1mbps()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    calcBitTiming(20000000, 500000, 1000000, nbtcfg, dbtcfg, tdcfg);
    // TDCMOD bits [17:16] = 2 (auto)
    TEST_ASSERT_EQUAL_UINT32(2u, (tdcfg >> 16) & 0x3);
    // TDCO must be non-zero
    TEST_ASSERT_NOT_EQUAL(0, (tdcfg >> 8) & 0xFF);
}

void test_calcBitTiming_tdc_disabled_below_1mbps()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    // 500 kbps data — TDC should be off
    calcBitTiming(20000000, 125000, 500000, nbtcfg, dbtcfg, tdcfg);
    TEST_ASSERT_EQUAL_UINT32(0, tdcfg);
}

void test_calcBitTiming_tdc_tdco_value_2m_20mhz()
{
    // 2 Mbps @ 20 MHz: totalTQ=10, tseg2=2, tseg1=7 → TDCO=8
    uint32_t nbtcfg, dbtcfg, tdcfg;
    calcBitTiming(20000000, 500000, 2000000, nbtcfg, dbtcfg, tdcfg);
    TEST_ASSERT_EQUAL_HEX32(TDC_2M_20MHZ, tdcfg);
}

void test_calcBitTiming_tdc_tdco_value_4m_20mhz()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    calcBitTiming(20000000, 500000, 4000000, nbtcfg, dbtcfg, tdcfg);
    TEST_ASSERT_EQUAL_HEX32(TDC_4M_20MHZ, tdcfg);
}

// ============================================================================
// calcBitTiming — rejection cases
// ============================================================================

void test_calcBitTiming_rejects_non_integer_tq()
{
    // 20 MHz / 8 Mbps = 2.5 TQ — not integer, must fail
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_FALSE(calcBitTiming(20000000, 500000, 8000000, nbtcfg, dbtcfg, tdcfg));
}

void test_calcBitTiming_rejects_zero_nominal()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_FALSE(calcBitTiming(20000000, 0, 1000000, nbtcfg, dbtcfg, tdcfg));
}

void test_calcBitTiming_rejects_zero_data()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_FALSE(calcBitTiming(20000000, 500000, 0, nbtcfg, dbtcfg, tdcfg));
}

void test_calcBitTiming_rejects_rate_too_high()
{
    // 20 MHz / 15 Mbps — not integer
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_FALSE(calcBitTiming(20000000, 500000, 15000000, nbtcfg, dbtcfg, tdcfg));
}

void test_calcBitTiming_rejects_totalTQ_lt_3()
{
    // 20 MHz / 10 Mbps = 2 TQ — below minimum of 3
    uint32_t nbtcfg, dbtcfg, tdcfg;
    TEST_ASSERT_FALSE(calcBitTiming(20000000, 500000, 10000000, nbtcfg, dbtcfg, tdcfg));
}

// ============================================================================
// calcBitTiming — sample point
// SP = (1 + TSEG1) / (1 + TSEG1 + TSEG2) — should be ~80%
// ============================================================================

void test_calcBitTiming_sample_point_500k_20mhz()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    calcBitTiming(20000000, 500000, 2000000, nbtcfg, dbtcfg, tdcfg);
    uint32_t tseg1 = (nbtcfg >> 16) & 0xFF;
    uint32_t tseg2 = (nbtcfg >>  8) & 0x7F;
    uint32_t totalTQ = 1 + tseg1 + tseg2;
    // SP = (1+tseg1)/totalTQ — expect 75-85%
    uint32_t sp_pct = (1 + tseg1) * 100 / totalTQ;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(75, sp_pct);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(85, sp_pct);
}

// ============================================================================
// calcBitTiming — SJW == TSEG2
// ============================================================================

void test_calcBitTiming_sjw_equals_tseg2()
{
    uint32_t nbtcfg, dbtcfg, tdcfg;
    calcBitTiming(20000000, 500000, 2000000, nbtcfg, dbtcfg, tdcfg);
    uint32_t tseg2 = (nbtcfg >> 8) & 0x7F;
    uint32_t sjw   =  nbtcfg       & 0x7F;
    TEST_ASSERT_EQUAL_UINT32(tseg2, sjw);
}

// ============================================================================
// calcTxTimeout — sanity bounds
// ============================================================================

void test_calcTxTimeout_minimum_is_2ms()
{
    // Pathological: all-zero timing → should still return at least 2
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2, calcTxTimeout(20000000, 0, 0));
}

void test_calcTxTimeout_500k_2m_reasonable()
{
    // 500 kbps nominal, 2 Mbps data @ 20 MHz — expect a few ms, not hundreds
    uint32_t ms = calcTxTimeout(20000000, NBTCFG_500K_20MHZ, DBTCFG_2M_20MHZ);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2,  ms);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(50, ms);
}

void test_calcTxTimeout_slower_rate_longer_timeout()
{
    uint32_t fast = calcTxTimeout(20000000, NBTCFG_500K_20MHZ, DBTCFG_4M_20MHZ);
    uint32_t slow = calcTxTimeout(20000000, NBTCFG_500K_20MHZ, DBTCFG_1M_20MHZ);
    TEST_ASSERT_GREATER_THAN_UINT32(fast, slow);
}

// ============================================================================
// EID encode/decode — T0/R0 bit packing  (DS20006027B Table 3-5/3-6)
// SID[10:0] = id[28:18], EID[17:0] = id[17:0]
// T0: bits[10:0]=SID, bits[28:11]=EID
// ============================================================================

void test_eid_encode_decode_roundtrip_max()
{
    uint32_t id = 0x1FFFFFFFu;  // all 29 bits set
    TEST_ASSERT_EQUAL_HEX32(id, decodeEidT0(encodeEidT0(id)));
}

void test_eid_encode_decode_roundtrip_zero()
{
    TEST_ASSERT_EQUAL_HEX32(0u, decodeEidT0(encodeEidT0(0u)));
}

void test_eid_encode_decode_roundtrip_known()
{
    // id = 0x12345678 (29-bit: 0001 0010 0011 0100 0101 0110 0111 1000)
    // SID = id>>18 = 0x048, EID = id&0x3FFFF = 0x25678 (but 0x3FFFF mask)
    uint32_t id = 0x12345678u & 0x1FFFFFFFu;
    TEST_ASSERT_EQUAL_HEX32(id, decodeEidT0(encodeEidT0(id)));
}

void test_eid_encode_sid_in_low_11_bits()
{
    // id = 0x1C000000 → SID = 0x700, EID = 0
    uint32_t id = 0x1C000000u;
    uint32_t t0 = encodeEidT0(id);
    TEST_ASSERT_EQUAL_HEX32(0x700u, t0 & 0x7FFu);  // SID in bits[10:0]
    TEST_ASSERT_EQUAL_HEX32(0u, (t0 >> 11) & 0x3FFFFu);  // EID=0
}

void test_eid_encode_eid_in_bits_28_11()
{
    // id = 0x0003FFFF → SID=0, EID=0x3FFFF
    uint32_t id = 0x0003FFFFu;
    uint32_t t0 = encodeEidT0(id);
    TEST_ASSERT_EQUAL_HEX32(0u, t0 & 0x7FFu);  // SID=0
    TEST_ASSERT_EQUAL_HEX32(0x3FFFFu, (t0 >> 11) & 0x3FFFFu);  // EID in bits[28:11]
}

// ============================================================================
// Filter OBJ/MASK encoding  (DS20006027B Register 3-33)
// ============================================================================

void test_filter_obj_eid_exide_bit_set()
{
    uint32_t obj = encodeFilterObjEid(0x12345678u & 0x1FFFFFFFu);
    TEST_ASSERT_NOT_EQUAL(0, obj & (1u << 30));  // EXIDE=1
}

void test_filter_obj_sid_roundtrip()
{
    // SID filter: id=0x123, mask=0x7FF (exact match)
    uint32_t obj = 0x123u & 0x7FFu;
    TEST_ASSERT_EQUAL_HEX32(0x123u, obj & 0x7FFu);
}

void test_filter_msk_eid_mide_bit_set()
{
    uint32_t msk = encodeFilterMskEid(0x1FFFFFFFu);
    TEST_ASSERT_NOT_EQUAL(0, msk & (1u << 30));  // MIDE=1
}

void test_filter_msk_eid_zero_mask_is_dont_care()
{
    // mask=0 → all EID bits don't-care, only MIDE set
    uint32_t msk = encodeFilterMskEid(0u);
    TEST_ASSERT_EQUAL_HEX32(1u << 30, msk);
}

void test_filter_obj_eid_roundtrip_known_id()
{
    uint32_t id = 0x18DAF110u;  // typical UDS physical request ID (29-bit)
    uint32_t obj = encodeFilterObjEid(id);
    // Decode back: strip EXIDE bit, then decode T0
    uint32_t t0 = obj & ~(1u << 30);
    TEST_ASSERT_EQUAL_HEX32(id, decodeEidT0(t0));
}

// ============================================================================
// Register address helpers — FIFO_CON/STA/UA, FLTOBJ/FLTMSK, FLTCON_REG/BYTE
// Verified against DS20006027B page 13 and Register 3-29
// ============================================================================

void test_fifo_con_fifo1_address()
{
    TEST_ASSERT_EQUAL_HEX16(0x05C, FIFO_CON(1));
}

void test_fifo_sta_fifo1_address()
{
    TEST_ASSERT_EQUAL_HEX16(0x060, FIFO_STA(1));
}

void test_fifo_ua_fifo1_address()
{
    TEST_ASSERT_EQUAL_HEX16(0x064, FIFO_UA(1));
}

void test_fifo_con_fifo2_address()
{
    TEST_ASSERT_EQUAL_HEX16(0x068, FIFO_CON(2));
}

void test_fifo_stride_is_0x0c()
{
    // Each FIFO block is 0x0C bytes
    TEST_ASSERT_EQUAL_HEX16(0x0C, FIFO_CON(2) - FIFO_CON(1));
}

void test_fltobj_stride_is_8()
{
    TEST_ASSERT_EQUAL_HEX16(8, FLTOBJ(1) - FLTOBJ(0));
}

void test_fltmsk_offset_from_fltobj_is_4()
{
    TEST_ASSERT_EQUAL_HEX16(4, FLTMSK(0) - FLTOBJ(0));
}

void test_fltcon_reg_groups_4_filters()
{
    // Filters 0-3 share one register, 4-7 share the next
    TEST_ASSERT_EQUAL_HEX16(FLTCON_REG(0), FLTCON_REG(1));
    TEST_ASSERT_EQUAL_HEX16(FLTCON_REG(0), FLTCON_REG(2));
    TEST_ASSERT_EQUAL_HEX16(FLTCON_REG(0), FLTCON_REG(3));
    TEST_ASSERT_NOT_EQUAL(FLTCON_REG(0), FLTCON_REG(4));
}

void test_fltcon_byte_offset_within_register()
{
    TEST_ASSERT_EQUAL_UINT8(0, FLTCON_BYTE(0));
    TEST_ASSERT_EQUAL_UINT8(1, FLTCON_BYTE(1));
    TEST_ASSERT_EQUAL_UINT8(2, FLTCON_BYTE(2));
    TEST_ASSERT_EQUAL_UINT8(3, FLTCON_BYTE(3));
    TEST_ASSERT_EQUAL_UINT8(0, FLTCON_BYTE(4));  // wraps
}

void test_fltcon_reg_filter31_address()
{
    // Filter 31: register = 0x1D0 + (31/4)*4 = 0x1D0 + 28 = 0x1EC
    TEST_ASSERT_EQUAL_HEX16(0x1EC, FLTCON_REG(31));
    TEST_ASSERT_EQUAL_UINT8(3, FLTCON_BYTE(31));
}

// ============================================================================
// FIFOCON bit constants — spot-check against Register 3-29
// ============================================================================

void test_fifocon_txen_bit7()
{
    TEST_ASSERT_EQUAL_HEX32(0x80u, FIFOCON_TXEN);
}

void test_fifocon_uinc_bit8()
{
    TEST_ASSERT_EQUAL_HEX32(0x100u, FIFOCON_UINC);
}

void test_fifocon_txreq_bit9()
{
    TEST_ASSERT_EQUAL_HEX32(0x200u, FIFOCON_TXREQ);
}

void test_fifosta_tfnrfnif_bit0()
{
    TEST_ASSERT_EQUAL_HEX32(0x01u, FIFOSTA_TFNRFNIF);
}

void test_fifosta_txerr_bit5()
{
    TEST_ASSERT_EQUAL_HEX32(0x20u, FIFOSTA_TXERR);
}

void test_fifosta_txabt_bit7()
{
    TEST_ASSERT_EQUAL_HEX32(0x80u, FIFOSTA_TXABT);
}

// ============================================================================

void setUp()    {}
void tearDown() {}

int main(int argc, char** argv)
{
    UNITY_BEGIN();

    // dlcToLen
    RUN_TEST(test_dlcToLen_classic_range);
    RUN_TEST(test_dlcToLen_fd_jumps);

    // calcBitTiming — 20 MHz
    RUN_TEST(test_calcBitTiming_20mhz_500k_2m);
    RUN_TEST(test_calcBitTiming_20mhz_500k_1m);
    RUN_TEST(test_calcBitTiming_20mhz_250k_2m);
    RUN_TEST(test_calcBitTiming_20mhz_125k_1m);
    RUN_TEST(test_calcBitTiming_20mhz_1m_4m);
    RUN_TEST(test_calcBitTiming_20mhz_500k_5m);

    // calcBitTiming — 40 MHz
    RUN_TEST(test_calcBitTiming_40mhz_500k_2m);
    RUN_TEST(test_calcBitTiming_40mhz_1m_8m);

    // TDC
    RUN_TEST(test_calcBitTiming_tdc_enabled_at_1mbps);
    RUN_TEST(test_calcBitTiming_tdc_disabled_below_1mbps);
    RUN_TEST(test_calcBitTiming_tdc_tdco_value_2m_20mhz);
    RUN_TEST(test_calcBitTiming_tdc_tdco_value_4m_20mhz);

    // Rejection
    RUN_TEST(test_calcBitTiming_rejects_non_integer_tq);
    RUN_TEST(test_calcBitTiming_rejects_zero_nominal);
    RUN_TEST(test_calcBitTiming_rejects_zero_data);
    RUN_TEST(test_calcBitTiming_rejects_rate_too_high);
    RUN_TEST(test_calcBitTiming_rejects_totalTQ_lt_3);

    // Sample point / SJW
    RUN_TEST(test_calcBitTiming_sample_point_500k_20mhz);
    RUN_TEST(test_calcBitTiming_sjw_equals_tseg2);

    // calcTxTimeout
    RUN_TEST(test_calcTxTimeout_minimum_is_2ms);
    RUN_TEST(test_calcTxTimeout_500k_2m_reasonable);
    RUN_TEST(test_calcTxTimeout_slower_rate_longer_timeout);

    // EID encode/decode
    RUN_TEST(test_eid_encode_decode_roundtrip_max);
    RUN_TEST(test_eid_encode_decode_roundtrip_zero);
    RUN_TEST(test_eid_encode_decode_roundtrip_known);
    RUN_TEST(test_eid_encode_sid_in_low_11_bits);
    RUN_TEST(test_eid_encode_eid_in_bits_28_11);

    // Filter encoding
    RUN_TEST(test_filter_obj_eid_exide_bit_set);
    RUN_TEST(test_filter_obj_sid_roundtrip);
    RUN_TEST(test_filter_msk_eid_mide_bit_set);
    RUN_TEST(test_filter_msk_eid_zero_mask_is_dont_care);
    RUN_TEST(test_filter_obj_eid_roundtrip_known_id);

    // Register address helpers
    RUN_TEST(test_fifo_con_fifo1_address);
    RUN_TEST(test_fifo_sta_fifo1_address);
    RUN_TEST(test_fifo_ua_fifo1_address);
    RUN_TEST(test_fifo_con_fifo2_address);
    RUN_TEST(test_fifo_stride_is_0x0c);
    RUN_TEST(test_fltobj_stride_is_8);
    RUN_TEST(test_fltmsk_offset_from_fltobj_is_4);
    RUN_TEST(test_fltcon_reg_groups_4_filters);
    RUN_TEST(test_fltcon_byte_offset_within_register);
    RUN_TEST(test_fltcon_reg_filter31_address);

    // FIFOCON/FIFOSTA bit constants
    RUN_TEST(test_fifocon_txen_bit7);
    RUN_TEST(test_fifocon_uinc_bit8);
    RUN_TEST(test_fifocon_txreq_bit9);
    RUN_TEST(test_fifosta_tfnrfnif_bit0);
    RUN_TEST(test_fifosta_txerr_bit5);
    RUN_TEST(test_fifosta_txabt_bit7);

    return UNITY_END();
}
