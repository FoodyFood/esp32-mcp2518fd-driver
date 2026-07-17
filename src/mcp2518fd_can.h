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
constexpr uint32_t NBTCFG_125K_40MHZ = 0x001E0707;  // 125 kbps  @ 40 MHz
constexpr uint32_t NBTCFG_250K_40MHZ = 0x000E0303;  // 250 kbps  @ 40 MHz
constexpr uint32_t NBTCFG_500K_40MHZ = 0x00060303;  // 500 kbps  @ 40 MHz
constexpr uint32_t NBTCFG_1M_40MHZ   = 0x00060101;  // 1 Mbps    @ 40 MHz

// Data bit rate (BRS phase — requires TDC at >= 1 Mbps):
constexpr uint32_t DBTCFG_1M_40MHZ   = 0x00260101;  // 1 Mbps    @ 40 MHz  (no TDC needed)
constexpr uint32_t DBTCFG_2M_40MHZ   = 0x00120101;  // 2 Mbps    @ 40 MHz  (use TDC_2M_40MHZ)

// ----------------------------------------------------------------------------
// TDC (Transmitter Delay Compensation) presets
//
// Required for data bit rates >= 1 Mbps. Use the preset that matches your
// DBTCFG selection. TDC is set to automatic mode — the chip measures the
// transmitter delay and applies the offset (TDCO) automatically.
//
// Formula: TDCO = (BRP+1) * (TSEG1+1)  [ref manual Table 3-5]
//
constexpr uint32_t TDC_DISABLED   = 0x00000000;  // TDC off  (use for < 1 Mbps data rate)
constexpr uint32_t TDC_1M_40MHZ   = 0x00022700;  // TDCMOD=auto TDCO=39  (1 Mbps  @ 40 MHz, TSEG1=38)
constexpr uint32_t TDC_2M_40MHZ   = 0x00021300;  // TDCMOD=auto TDCO=19  (2 Mbps  @ 40 MHz, TSEG1=18)

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
    // Returns false if the TX FIFO is full or the controller does not
    // acknowledge within 10 ms.
    bool transmit(const CanMsg& msg);

    // Read one CAN FD frame from the RX FIFO.
    // Waits up to 10 ms for a frame to arrive.
    // Returns true and populates msg on success.
    // Returns false if no frame arrives within the timeout.
    bool receive(CanMsg& msg);

    // Return the current operating mode (OPMOD field of CiCON).
    // Compare against MODE_* constants from mcp2518fd_registers.h.
    uint8_t getMode();

private:
    MCP2518SPI mSpi;

    void     configFifos();
    void     configFilter();
    uint16_t txRamAddr();
    uint16_t rxRamAddr();
};
