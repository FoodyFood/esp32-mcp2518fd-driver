#pragma once
#include <stdint.h>
#include "mcp2518fd_can.h"

// Frame IDs — sourced from MEB-BATTERY.h
// 11-bit SIDs
static constexpr uint32_t ID_BMS_20            = 0x0CF;
static constexpr uint32_t ID_BMS_04            = 0x5A2;
static constexpr uint32_t ID_BMS_07            = 0x5CA;
static constexpr uint32_t ID_BMS_DC_01         = 0x578;
static constexpr uint32_t ID_BMS_HYB_02        = 0x097;
static constexpr uint32_t ID_BMS_HYB_04        = 0x124;
static constexpr uint32_t ID_MSG_HYB_30        = 0x153;
static constexpr uint32_t ID_HVLM_13           = 0x271;
static constexpr uint32_t ID_HVLM_14           = 0x272;
static constexpr uint32_t ID_HVK_01            = 0x503;
// 29-bit EIDs
static constexpr uint32_t ID_BMS_21            = 0x12DD54D0;
static constexpr uint32_t ID_BMS_22            = 0x12DD54D1;
static constexpr uint32_t ID_BMS_24            = 0x1A555550;
static constexpr uint32_t ID_BMS_11            = 0x16A954A6;
static constexpr uint32_t ID_KN_Hybrid_01      = 0x17F0007B;
static constexpr uint32_t ID_NMH_Hybrid_01     = 0x1B00007B;  // classic CAN

// VAG PDU constant tables — copied verbatim from MEB-BATTERY.cpp
// Used by vw_crc_calc() to produce the magic byte for each counter value.
static const uint8_t BMS_20_PDU_CONST[16] = {
    0xee, 0x80, 0x6e, 0x4e, 0x29, 0xc6, 0x92, 0xc0,
    0x65, 0xaa, 0x3a, 0xa1, 0x8f, 0xcd, 0xe6, 0x90
};
static const uint8_t BMS_04_PDU_CONST[16] = {
    0xeb, 0x4c, 0x44, 0xaf, 0x21, 0x8d, 0x01, 0x58,
    0xfa, 0x93, 0xdb, 0x89, 0x15, 0x10, 0x4a, 0x61
};
static const uint8_t BMS_07_PDU_CONST[16] = {
    0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43,
    0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43
};

// VAG CRC — 8-bit, polynomial 0x2F, init 0xFF, XOR output 0xFF.
// Ported directly from MEB-BATTERY.cpp vw_crc_calc().
// Writes the result into data[0] in-place.
// counter is the low nibble of data[1].
static void vw_crc_apply(uint8_t* data, uint8_t length, const uint8_t* pdu_const)
{
    constexpr uint8_t poly       = 0x2F;
    constexpr uint8_t xor_output = 0xFF;

    uint8_t counter  = data[1] & 0x0F;
    uint8_t magicByte = pdu_const[counter];

    uint8_t crc = 0xFF;
    for (uint8_t i = 1; i < length + 1; i++) {
        crc ^= (i < length) ? data[i] : magicByte;
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x80) ? (crc << 1) ^ poly : (crc << 1);
    }
    data[0] = crc ^ xor_output;
}

// ---------------------------------------------------------------------------
// Frame builders
// Each function returns a CanMsg ready to transmit.
// Mutable state (counters, dynamic fields) is passed in by the caller.
// ---------------------------------------------------------------------------

// BMS_20 — 10 ms — signals BMS mode and contactor state to the inverter.
// BMS_mode=1 (HV_ACTIVE) tells Battery-Emulator the contactors are closed.
// Sourced from MEB-BATTERY.h frame definitions and MEB-BATTERY.cpp transmit_can().
static CanMsg make_BMS_20(uint8_t counter, uint16_t voltage_raw, uint16_t current_raw)
{
    CanMsg msg = {};
    msg.id  = ID_BMS_20;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    // byte 1: [7:4] upper nibble reserved, [3:0] rolling counter
    msg.data[1] = counter & 0x0F;
    // byte 2: BMS_mode=1 (HV_ACTIVE) in bits [2:0], HVIL_status=1 (seated) in bits [4:3]
    msg.data[2] = 0x01 | (1 << 3);
    // byte 3-4: current (raw, 16300 = 0A offset from MEB-BATTERY.h)
    msg.data[3] = current_raw & 0xFF;
    msg.data[4] = (current_raw >> 8) & 0x7F;  // bit7 reserved
    // byte 5-6: intermediate voltage (raw)
    msg.data[5] = voltage_raw & 0xFF;
    msg.data[6] = (voltage_raw >> 8) & 0x0F;
    // byte 7: pack voltage high nibble
    msg.data[7] = (voltage_raw >> 4) & 0xFF;

    vw_crc_apply(msg.data, msg.dlc, BMS_20_PDU_CONST);
    return msg;
}

// BMS_22 — 100 ms — SOC and usable energy.
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() BMS_22 decode.
// We encode the inverse: pack SOC and energy into the same bit positions.
static CanMsg make_BMS_22(uint16_t soc_raw, uint16_t energy_wh_raw)
{
    CanMsg msg = {};
    msg.id  = ID_BMS_22;
    msg.ext = true;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    // SOC: bits [10:0] of bytes [3:2], shifted per decode: battery_SOC = ((data[3]&0x0F)<<7)|(data[2]>>1)
    msg.data[2] = (soc_raw & 0x7F) << 1;
    msg.data[3] = (soc_raw >> 7) & 0x0F;
    // usable energy: bytes [7:6]
    msg.data[6] = energy_wh_raw & 0xFF;
    msg.data[7] = (energy_wh_raw >> 8) & 0xFF;
    // status_HV_line=1 (no open HV line) in bits [1:0] of bytes [2:1]
    msg.data[1] = 0x80;  // bit7 of byte1 = status_HV_line LSB = 1
    msg.data[2] |= 0x01; // bit0 of byte2 = status_HV_line MSB = 0 → value = 01 = no open line

    return msg;
}

// BMS_21 — 100 ms — charge/discharge power limits.
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() BMS_21 decode.
static CanMsg make_BMS_21(uint16_t max_charge_kw, uint16_t max_discharge_kw)
{
    CanMsg msg = {};
    msg.id  = ID_BMS_21;
    msg.ext = true;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    // max_charge_power_watt = (data[7]<<5)|(data[6]>>3)  *100
    // max_discharge_power_watt = ((data[6]&0x07)<<10)|(data[5]<<2)|((data[4]&0xC0)>>6)  *100
    uint16_t charge_raw    = (max_charge_kw * 10);      // *100 factor, kW→W/100
    uint16_t discharge_raw = (max_discharge_kw * 10);

    msg.data[7] = (charge_raw >> 5) & 0xFF;
    msg.data[6] = ((charge_raw & 0x1F) << 3) | ((discharge_raw >> 10) & 0x07);
    msg.data[5] = (discharge_raw >> 2) & 0xFF;
    msg.data[4] = (discharge_raw & 0x03) << 6;

    return msg;
}

// BMS_04 — 100 ms — contactor status, capacity, error flags.
// BMS_Kl30c_Status=1 (closed) in bits [5:4] of byte 4 tells Battery-Emulator
// the 12V supply is active. Without it, capacity is ignored.
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() BMS_04 decode.
static CanMsg make_BMS_04(uint8_t counter, uint16_t capacity_ah)
{
    CanMsg msg = {};
    msg.id  = ID_BMS_04;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    msg.data[1] = counter & 0x0F;
    // BMS_status_voltage_free=2 (not voltage-free, HV present) in bits [7:6] of byte 1
    msg.data[1] |= (2 << 6);
    // BMS_error_status=0 (Component_IO, no fault) in bits [6:4] of byte 2
    msg.data[2] = 0x00;
    // capacity_ah: bits [10:0] packed across bytes [4:2]
    // decode: BMS_capacity_ah = ((data[4]&0x03)<<9)|(data[3]<<1)|(data[2]>>7)
    msg.data[2] |= (capacity_ah & 0x01) << 7;
    msg.data[3]  = (capacity_ah >> 1) & 0xFF;
    msg.data[4]  = (capacity_ah >> 9) & 0x03;
    // BMS_Kl30c_Status=1 (closed) in bits [5:4] of byte 4
    msg.data[4] |= (1 << 4);

    vw_crc_apply(msg.data, msg.dlc, BMS_04_PDU_CONST);
    return msg;
}

// BMS_07 — 500 ms — battery diagnostic and potential status.
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() BMS_07 decode.
static CanMsg make_BMS_07(uint8_t counter)
{
    CanMsg msg = {};
    msg.id  = ID_BMS_07;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    msg.data[1] = counter & 0x0F;
    // battery_potential_status=2 (potential on) in bits [5:4] of byte 5
    msg.data[5] = (2 << 4);

    vw_crc_apply(msg.data, msg.dlc, BMS_07_PDU_CONST);
    return msg;
}

// KN_Hybrid_01 — 500 ms — network management, wakeup reason.
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() KN_Hybrid_01 decode.
// KL15_mode=2 (communication when KL15 OFF, can be woken up) in bits [7:4] of byte 0.
static CanMsg make_KN_Hybrid_01()
{
    CanMsg msg = {};
    msg.id  = ID_KN_Hybrid_01;
    msg.ext = true;
    msg.fdf = true;
    msg.brs = true;
    msg.dlc = 8;

    msg.data[0] = (2 << 4);  // KL15_mode=2
    return msg;
}

// NMH_Hybrid_01 — 200 ms — classic CAN 2.0B network management frame.
// FD=false exercises the mixed-mode capability (SPEC-008).
// Sourced from MEB-BATTERY.cpp handle_incoming_can_frame() NMH_Hybrid_01 decode.
static CanMsg make_NMH_Hybrid_01()
{
    CanMsg msg = {};
    msg.id  = ID_NMH_Hybrid_01;
    msg.ext = true;
    msg.fdf = false;  // classic CAN — not FD
    msg.brs = false;
    msg.dlc = 8;

    // wakeup_type=1 (active, SG has woken up the network) in bit [4] of byte 1
    msg.data[1] = (1 << 4);
    return msg;
}
