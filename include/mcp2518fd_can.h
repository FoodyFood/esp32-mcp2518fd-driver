#pragma once

#include <Arduino.h>
#include "mcp2518fd_spi.h"
#include "mcp2518fd_registers.h"

// ----------------------------------------------------------------------------
// CanStatus — returned by configure() and setDataRate()
//
// Check for CanStatus::OK before transmitting. All other values indicate
// the chip is not in the requested mode and should not be used.
enum class CanStatus : uint8_t
{
    OK = 0,
    MODE_TIMEOUT,         // chip did not confirm the requested mode
    RATE_NOT_ACHIEVABLE,  // target bit rate cannot be reached at the detected FSYS
    CLOCK_NOT_READY,      // OSC register shows clock not stable after reset
};

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
    uint32_t id   = 0;     // Frame identifier: 11-bit SID (ext=false) or 29-bit EID (ext=true)
    bool     ext  = false; // false = standard 11-bit frame, true = extended 29-bit frame
    bool     fdf  = false; // true = CAN FD frame, false = Classic CAN
    bool     brs  = false; // true = switch to data bit rate in payload phase
    uint8_t  dlc  = 0;     // Data Length Code (0–15)
    uint8_t  data[64] = {}; // Payload bytes (up to 64 for CAN FD)
};

// ----------------------------------------------------------------------------
// MCP2518FD driver
//
// Typical usage — just specify the rates you want, the driver handles the rest:
//
//   MCP2518Driver can(spi, PIN_CS);
//   can.configure(125000, 2000000, MODE_NORMAL);   // 125 kbps nominal, 2 Mbps data
//
//   CanMsg tx = { .id=0x123, .fdf=true, .brs=true, .dlc=8 };
//   for (int i = 0; i < 8; i++) tx.data[i] = i;
//   can.transmit(tx);
//
//   CanMsg rx;
//   can.receive(rx, 500);  // blocking, 500ms timeout
//
class MCP2518Driver
{
public:
    MCP2518Driver(SPIClass& spi, uint8_t csPin);

    // Reset the chip, auto-detect oscillator frequency, calculate and apply
    // bit timing for the requested nominal and data rates, configure
    // FIFO1=TX / FIFO2=RX, enable a catch-all acceptance filter, then enter
    // the requested mode.
    //
    // nominalBps — arbitration phase rate in bps (e.g. 125000, 250000, 500000, 1000000)
    // dataBps    — data phase rate in bps       (e.g. 1000000, 2000000, 4000000, 5000000)
    // mode       — MODE_NORMAL, MODE_INTERNAL_LB, MODE_EXTERNAL_LB, etc.
    //
    // Returns CanStatus::OK when the chip confirms the requested mode.
    CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode);

    // Change the data bit rate at runtime without disturbing the nominal rate.
    // Performs a config-mode round-trip and returns to the previous mode.
    CanStatus setDataRate(uint32_t dataBps);

    // Write one CAN FD frame into the TX FIFO and request transmission.
    // Returns true when the frame is accepted by the controller.
    bool transmit(const CanMsg& msg);

    // Returns true if at least one frame is waiting in the RX FIFO (non-blocking).
    bool available();

    // Non-blocking receive — returns true immediately if a frame is waiting.
    bool receive(CanMsg& msg);

    // Blocking receive with explicit timeout in milliseconds.
    bool receive(CanMsg& msg, uint32_t timeoutMs);

    // Return the current operating mode (OPMOD field of CiCON).
    // Compare against MODE_* constants from mcp2518fd_registers.h.
    uint8_t getMode();

    // Return the detected oscillator frequency in Hz (typically 20000000 or 40000000).
    // Valid after configure() has been called.
    uint32_t getFsys() const { return mFsys; }

    // Read the raw OSC register — useful for diagnostics.
    uint32_t readOsc();

    // ------------------------------------------------------------------------
    // Raw / advanced API
    //
    // Use these only if you need direct control over register values, e.g. for
    // non-standard rates or oscillator frequencies not covered by the auto-
    // calculation. Presets for common rates are defined below.
    //
    // configureRaw() and setDataBitTimingRaw() bypass auto-detection and
    // accept pre-computed register words directly.
    CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode);
    CanStatus setDataBitTimingRaw(uint32_t dbtcfg, uint32_t tdcfg);

private:
    MCP2518SPI mSpi;
    uint32_t   mFsys        = 0;
    uint32_t   mTxTimeoutMs = 10;
    uint32_t   mNbtcfg      = 0;

    // Detect FSYS from the OSC register after reset.
    // Returns frequency in Hz, or 0 if the clock is not ready.
    uint32_t detectFsys();

    // Calculate NBTCFG, DBTCFG and TDCFG register values for the given rates
    // and oscillator frequency. Maximises TSEG1 for best noise margin at 80%
    // sample point. Returns false if the rate is not achievable.
    // Formula: rate = fsys / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-1/3-4]
    bool calcBitTiming(uint32_t fsys, uint32_t nominalBps, uint32_t dataBps,
                       uint32_t& nbtcfg, uint32_t& dbtcfg, uint32_t& tdcfg);

    void calcTxTimeout(uint32_t nbtcfg, uint32_t dbtcfg);
    void configFifos();
    void configFilter();
    void applyTiming(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg);
    uint16_t txRamAddr();
    uint16_t rxRamAddr();
};

// ----------------------------------------------------------------------------
// Bit timing presets — advanced use only
//
// These are pre-computed register words for common rates and oscillator
// frequencies. Prefer configure(nominalBps, dataBps, mode) which selects
// the correct values automatically.
//
// Formula: rate = FSYS / ((BRP+1) * (1 + TSEG1 + TSEG2))  [ref manual Eq 3-1/3-4]
// All presets use BRP=0, exact rate, 80% sample point (75% for 5 Mbps).
// Verified against tools/check_timing.py.
//
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
