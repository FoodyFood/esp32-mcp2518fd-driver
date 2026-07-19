#pragma once

#include <Arduino.h>
#include "mcp2518fd_spi.h"
#include "mcp2518fd_registers.h"
#include "mcp2518fd_timing.h"

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
    uint32_t id        = 0;     // Frame identifier: 11-bit SID (ext=false) or 29-bit EID (ext=true)
    bool     ext       = false; // false = standard 11-bit frame, true = extended 29-bit frame
    bool     fdf       = false; // true = CAN FD frame, false = Classic CAN
    bool     brs       = false; // true = switch to data bit rate in payload phase
    uint8_t  dlc       = 0;     // Data Length Code (0–15)
    uint32_t timestamp = 0;     // RX hardware timestamp (TBC counts); 0 when timestamping not enabled
    uint8_t  data[64]  = {};    // Payload bytes (up to 64 for CAN FD)
};

// ----------------------------------------------------------------------------
// CanTxResult — returned by transmit()
enum class CanTxResult : uint8_t
{
    OK,       // frame transmitted and ACKed
    NoAck,    // chip retried 3x, no ACK received (other node absent or bus disconnected)
    BusError, // TXERR set — bit error, stuff error, etc.
    FifoFull, // TX FIFO had no space (TFNRFNIF was clear)
};

// ----------------------------------------------------------------------------
// CanError — returned by getErrors()
struct CanError
{
    uint8_t tec;        // transmit error counter (CiTREC bits 15:8)
    uint8_t rec;        // receive error counter  (CiTREC bits 7:0)
    bool    txWarning;  // TEC >= 96
    bool    rxWarning;  // REC >= 96
    bool    txPassive;  // TEC >= 128
    bool    rxPassive;  // REC >= 128
    bool    busOff;     // TEC > 255 — node is bus-off
    bool    rxOverflow; // at least one RX FIFO overflowed since last call
};


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
    // intPin — GPIO pin connected to MCP2518FD INT (active-low). Pass -1 (default)
    //          to use polling-only mode. When >= 0, an ISR is attached in configure()
    //          and available() returns true immediately on frame arrival.
    MCP2518Driver(SPIClass& spi, uint8_t csPin, int8_t intPin = -1);

    // Reset the chip, auto-detect oscillator frequency, calculate and apply
    // bit timing for the requested nominal and data rates, configure
    // FIFO1=TX / FIFO2=RX, enable a catch-all acceptance filter, then enter
    // the requested mode.
    //
    // nominalBps  — arbitration phase rate in bps (e.g. 125000, 250000, 500000, 1000000)
    // dataBps     — data phase rate in bps       (e.g. 1000000, 2000000, 4000000, 5000000)
    // mode        — MODE_NORMAL, MODE_INTERNAL_LB, MODE_EXTERNAL_LB, etc.
    // rxFifoDepth — RX FIFO slot count (1–23 at PLSIZE_64 with timestamp, 1–24 without).
    //               Default 16. Clamped to max.
    //               RAM budget: FIFO1 (TX, depth=4) = 288 bytes; remaining = 1760 bytes.
    //               Without timestamp: slot = 72 bytes → max 24. With: slot = 76 bytes → max 23.
    // enableTimestamp — when true, enables the free-running CiTBC counter and sets RXTSEN in
    //               FIFO2 so each received message object includes a 32-bit hardware timestamp.
    //               The timestamp is captured at SOF (TSEOF=0, TSRES=0). Resolution: 1 FSYS clock
    //               (50 ns at 20 MHz). Populated in msg.timestamp after receive().
    //
    // Returns CanStatus::OK when the chip confirms the requested mode.
    CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode,
                        uint8_t rxFifoDepth = 16, bool enableTimestamp = false);

    // Change the data bit rate at runtime without disturbing the nominal rate.
    // Performs a config-mode round-trip and returns to the previous mode.
    CanStatus setDataRate(uint32_t dataBps);

    // Write one CAN FD frame into the TX FIFO and request transmission.
    // Returns CanTxResult::OK when the frame is transmitted and ACKed.
    // Returns NoAck if all retransmission attempts exhausted with no ACK.
    // Returns BusError if a bus error was detected during transmission.
    // Returns FifoFull if the TX FIFO had no space.
    CanTxResult transmit(const CanMsg& msg);

    // Read TEC/REC counters and error flags from CiTREC and CiRXOVIF.
    // Clears the rxOverflow flag after reading.
    CanError getErrors();

    // Returns true if TEC or REC >= 96, or busOff, or rxOverflow.
    // Cheaper than getErrors() — suitable for polling in a loop.
    bool hasErrors();

    // Returns true if at least one frame is waiting in the RX FIFO (non-blocking).
    bool available();

    // Non-blocking receive — returns true immediately if a frame is waiting.
    bool receive(CanMsg& msg);

    // Blocking receive with explicit timeout in milliseconds.
    bool receive(CanMsg& msg, uint32_t timeoutMs);

    // Configure an acceptance filter.
    // index  0–31 selects the filter slot.
    // id     identifier to match (11-bit SID or 29-bit EID depending on ext).
    // mask   bits set to 1 are compared; bits 0 are don’t-care.
    // ext    false = match standard frames (MIDE=1, EXIDE=0 not set)
    //        true  = match extended frames only (MIDE=1, EXIDE=1)
    //        Pass ext=false with mask=0 to match all frame types (catch-all).
    // All matched frames route to FIFO2.
    // Safe to call in normal mode — disables filter, updates, re-enables.
    void setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext);

    // Disable a filter slot without changing its OBJ/MASK registers.
    void clearFilter(uint8_t index);

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
    CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode,
                            uint8_t rxFifoDepth = 16, bool enableTimestamp = false);
    CanStatus setDataBitTimingRaw(uint32_t dbtcfg, uint32_t tdcfg);

private:
    MCP2518SPI mSpi;
    uint32_t   mFsys        = 0;
    uint32_t   mTxTimeoutMs = 10;
    uint32_t   mNbtcfg      = 0;
    int8_t     mIntPin      = -1;
    bool       mTimestamp   = false;  // true when RXTSEN + CiTBC are enabled
    volatile bool mRxPending = false;

    static MCP2518Driver* sIsrInstance;  // single-instance ISR trampoline
    static void IRAM_ATTR sIsrHandler();

    // Detect FSYS from the OSC register after reset.
    // Returns frequency in Hz, or 0 if the clock is not ready.
    uint32_t detectFsys();

    void configFifos(uint8_t rxFifoDepth, bool enableTimestamp);
    void configFilter();  // installs catch-all filter 0 → FIFO2
    void applyTiming(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg);
    uint16_t txRamAddr();
    uint16_t rxRamAddr();
};

// Bit timing presets — see mcp2518fd_presets.h
#include "mcp2518fd_presets.h"
