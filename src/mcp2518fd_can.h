#pragma once

#include <Arduino.h>
#include "mcp2518fd_spi.h"
#include "mcp2518fd_registers.h"

// ----------------------------------------------------------------------------
// Bit timing presets
//
// Pass one of these to configure() or setDataBitTiming().
// Each constant encodes BRP, TSEG1, TSEG2 and SJW for a specific rate and
// oscillator frequency. Add new presets here as needed — do not pass raw
// register values from application code.
//
// Nominal bit rate (arbitration phase):
// Formula: NBR = FSYS / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-1/3-3]
// All presets use ~80% sample point, verified against tools/check_timing.py.
constexpr uint32_t NBTCFG_125K_40MHZ = 0x071F0808;  // 125 kbps  @ 40 MHz  BRP=7 TSEG1=31 TSEG2=8  SJW=8
constexpr uint32_t NBTCFG_250K_40MHZ = 0x031F0808;  // 250 kbps  @ 40 MHz  BRP=3 TSEG1=31 TSEG2=8  SJW=8
constexpr uint32_t NBTCFG_500K_40MHZ = 0x011F0808;  // 500 kbps  @ 40 MHz  BRP=1 TSEG1=31 TSEG2=8  SJW=8
constexpr uint32_t NBTCFG_1M_40MHZ   = 0x001F0808;  // 1 Mbps    @ 40 MHz  BRP=0 TSEG1=31 TSEG2=8  SJW=8

// Data bit rate (BRS phase — requires TDC at >= 1 Mbps):
// Formula: DBR = FSYS / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-2/3-4]
constexpr uint32_t DBTCFG_1M_40MHZ   = 0x00260101;  // 1 Mbps    @ 40 MHz  BRP=0 TSEG1=38 TSEG2=1 SJW=1  (TDC optional)
constexpr uint32_t DBTCFG_2M_40MHZ   = 0x00120101;  // 2 Mbps    @ 40 MHz  BRP=0 TSEG1=18 TSEG2=1 SJW=1
constexpr uint32_t DBTCFG_4M_40MHZ   = 0x00070202;  // 4 Mbps    @ 40 MHz  BRP=0 TSEG1=7  TSEG2=2 SJW=2
constexpr uint32_t DBTCFG_5M_40MHZ   = 0x00050202;  // 5 Mbps    @ 40 MHz  BRP=0 TSEG1=5  TSEG2=2 SJW=2
constexpr uint32_t DBTCFG_8M_40MHZ   = 0x00030101;  // 8 Mbps    @ 40 MHz  BRP=0 TSEG1=3  TSEG2=1 SJW=1

// ----------------------------------------------------------------------------
// TDC (Transmitter Delay Compensation) presets
//
// Required for data bit rates >= 1 Mbps. Use the preset that matches your
// DBTCFG selection. TDC is set to automatic mode — the chip measures the
// transmitter delay and applies the offset (TDCO) automatically.
//
// Formula: TDCO = (BRP+1) * (TSEG1+1)  [ref manual Table 3-5]
//
// TDCO = (BRP+1) * (TSEG1+1)  [ref manual Table 3-5, verified example: BRP=0 TSEG1=14 -> TDCO=15]
constexpr uint32_t TDC_DISABLED   = 0x00000000;  // TDC off  (use for < 1 Mbps data rate)
constexpr uint32_t TDC_1M_40MHZ   = 0x00022700;  // TDCMOD=auto TDCO=39  (1 Mbps  @ 40 MHz, TSEG1=38)
constexpr uint32_t TDC_2M_40MHZ   = 0x00021300;  // TDCMOD=auto TDCO=19  (2 Mbps  @ 40 MHz, TSEG1=18)
constexpr uint32_t TDC_4M_40MHZ   = 0x00020800;  // TDCMOD=auto TDCO=8   (4 Mbps  @ 40 MHz, TSEG1=7)
constexpr uint32_t TDC_5M_40MHZ   = 0x00020600;  // TDCMOD=auto TDCO=6   (5 Mbps  @ 40 MHz, TSEG1=5)
constexpr uint32_t TDC_8M_40MHZ   = 0x00020400;  // TDCMOD=auto TDCO=4   (8 Mbps  @ 40 MHz, TSEG1=3)

// ----------------------------------------------------------------------------
// CAN message
//
// Used for both transmit and receive. On transmit, populate all fields.
// On receive, all fields are filled in by receive().
//
// CAN FD DLC → byte length (DS20006027B Table 3-3)
// DLC 0-8 map 1:1; DLC 9=12, 10=16, 11=20, 12=24, 13=32, 14=48, 15=64
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

struct CanMsg
{
    uint16_t sid;       // Standard identifier, 11-bit (0x000–0x7FF)
    bool     fdf;       // true = CAN FD frame, false = Classic CAN
    bool     brs;       // true = switch to data bit rate in payload phase
    uint8_t  dlc;       // Data Length Code (0–15)
    uint8_t  data[64];  // Payload bytes (up to 64 for CAN FD)
};

// ----------------------------------------------------------------------------
// MCP2518FD driver
//
// Single-responsibility: owns the chip lifecycle from reset through
// transmit/receive. All register-level detail is internal.
//
// Typical usage:
//
//   MCP2518Driver can(spi, PIN_CS);
//   can.configure(NBTCFG_125K_40MHZ, DBTCFG_2M_40MHZ, TDC_2M_40MHZ, MODE_INTERNAL_LB);
//
//   CanMsg tx = { .sid=0x123, .fdf=true, .brs=true, .dlc=8, .data={1,2,3,4,5,6,7,8} };
//   can.transmit(tx);
//
//   CanMsg rx;
//   can.receive(rx);
//
class MCP2518Driver
{
public:
    MCP2518Driver(SPIClass& spi, uint8_t csPin);

    // Reset the chip, apply bit timing and TDC, configure FIFO1=TX / FIFO2=RX,
    // enable a catch-all acceptance filter, then enter the requested mode.
    // Returns true when the chip confirms the requested mode via OPMOD.
    bool configure(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode);

    // Change the data bit rate at runtime without disturbing the nominal rate.
    // Performs a config-mode round-trip, rewrites DBTCFG + TDC, re-enables the
    // acceptance filter, then returns to the previous mode.
    // Returns true when the chip confirms the mode.
    bool setDataBitTiming(uint32_t dbtcfg, uint32_t tdcfg);

    // Write one CAN FD frame into the TX FIFO and request transmission.
    // Returns true when TXREQ clears (frame accepted by the controller).
    // Timeout is derived from the configured bit timing — worst-case 64-byte
    // frame × 3 retransmission attempts + 2ms margin.
    bool transmit(const CanMsg& msg);

    // Returns true if at least one frame is waiting in the RX FIFO (non-blocking).
    bool available();

    // Read one CAN FD frame from the RX FIFO (non-blocking).
    // Returns true immediately if a frame is waiting, false if the FIFO is empty.
    // For a blocking version with timeout, use receive(msg, timeout_ms).
    bool receive(CanMsg& msg);

    // Blocking receive with explicit timeout in milliseconds.
    bool receive(CanMsg& msg, uint32_t timeoutMs);

    // Return the current operating mode (OPMOD field of CiCON).
    // Compare against MODE_* constants from mcp2518fd_registers.h.
    uint8_t getMode();

private:
    MCP2518SPI mSpi;
    uint32_t   mTxTimeoutMs = 10;  // recalculated by calcTxTimeout() on every configure/setDataBitTiming
    uint32_t   mNbtcfg      = 0;

    // Derive worst-case TX timeout from configured bit timing.
    // Worst case: 64-byte CAN FD frame (BRS=true), 3 retransmission attempts.
    //
    // Frame bit budget (standard 11-bit ID, BRS=true, ref ISO 11898-1):
    //   Nominal phase: SOF(1) + arb(11) + ctrl(6) + CRC-delim(1) + ACK(2) + EOF+IFS(10) + stuff(~5) = ~36 bits
    //   Data phase:    ESI(1) + DLC(4) + data(512) + stuff-count(4) + CRC21(21) + stuff(~20) = ~562 bits
    //
    // NBT_ns = (BRP+1) * (1 + TSEG1 + TSEG2) * 25  [ns, at 40 MHz]
    // DBT_ns = (BRP+1) * (1 + TSEG1 + TSEG2) * 25  [ns, at 40 MHz]
    // frame_us = (36 * NBT_ns + 562 * DBT_ns) / 1000
    // timeout_ms = ceil(frame_us * 3 / 1000) + 2   [3 attempts + 2ms margin]
    void calcTxTimeout(uint32_t nbtcfg, uint32_t dbtcfg);

    void     configFifos();
    void     configFilter();
    uint16_t txRamAddr();
    uint16_t rxRamAddr();
};
