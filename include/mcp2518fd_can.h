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
    INVALID_MODE,         // operation not valid in the current mode (e.g. FD frame in MODE_CLASSIC)
};

// ----------------------------------------------------------------------------
// CAN message — used for both transmit and receive.
struct CanMsg
{
    uint32_t id        = 0;     // Frame identifier: 11-bit SID (ext=false) or 29-bit EID (ext=true)
    bool     ext       = false; // false = standard 11-bit frame, true = extended 29-bit frame
    bool     fdf       = false; // true = CAN FD frame, false = Classic CAN
    bool     brs       = false; // true = switch to data bit rate in payload phase (only meaningful when fdf=true)
    uint8_t  dlc       = 0;     // Data Length Code (0–15)
    uint32_t timestamp = 0;     // RX hardware timestamp (TBC counts); 0 when timestamping not enabled
    uint8_t  data[64]  = {};    // Payload bytes (up to 64 for CAN FD)
};

// ----------------------------------------------------------------------------
// CanTxResult — returned by transmit()
enum class CanTxResult : uint8_t
{
    OK,          // frame transmitted and ACKed
    NoAck,       // chip retried 3x, no ACK received (other node absent or bus disconnected)
    BusError,    // TXERR set — bit error, stuff error, etc.
    FifoFull,    // TX FIFO had no space (TFNRFNIF was clear)
    InvalidMode, // operation not valid in the current mode (e.g. FD frame in MODE_CLASSIC)
};

// ----------------------------------------------------------------------------
// CanError — returned by readAndClearErrors()
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

// ----------------------------------------------------------------------------
// CanConfig — optional settings for configure() and configureRaw()
//
// Pass as the fourth argument when you need non-default FIFO depth or
// hardware timestamps. All fields have safe defaults so omitting it gives
// the same behaviour as the 3-argument form.
struct CanConfig
{
    uint8_t rxFifoDepth     = 16;    // RX FIFO slot count (1–23 with timestamp, 1–24 without)
    bool    enableTimestamp = false; // enable 32-bit hardware timestamp on each received frame

    CanConfig() = default;
    explicit CanConfig(uint8_t depth, bool ts = false) : rxFifoDepth(depth), enableTimestamp(ts) {}
};

// GPIO sentinel — pass as intPin when no interrupt pin is connected
constexpr int8_t NO_INT_PIN = -1;

// Typical usage:
//   MCP2518Driver can(spi, PIN_CS);
//   can.configure(500000, 2000000, MODE_NORMAL);
//
//   CanMsg tx = { .id=0x123, .fdf=true, .brs=true, .dlc=8 };
//   can.transmit(tx);
//
//   CanMsg rx;
//   can.receive(rx, 500);  // blocking, 500 ms timeout
//
class MCP2518Driver
{
public:
    // intPin — GPIO connected to MCP2518FD INT (active-low).
    //          Pass NO_INT_PIN (default) for polling-only mode.
    MCP2518Driver(SPIClass& spi, uint8_t csPin, int8_t intPin = NO_INT_PIN);

    // Reset, auto-detect oscillator, calculate bit timing, configure FIFOs and
    // catch-all filter, then enter the requested mode.
    // cfg is optional — omit for default FIFO depth (16) and no timestamps.
    CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode);
    CanStatus configure(uint32_t nominalBps, uint32_t dataBps, uint8_t mode, const CanConfig& cfg);

    // Change the data bit rate at runtime without disturbing the nominal rate.
    // Returns INVALID_MODE if the driver is in MODE_CLASSIC (no data phase).
    CanStatus setDataRate(uint32_t dataBps);

    // Transmit one frame. Returns OK when ACKed, NoAck/BusError/FifoFull otherwise.
    CanTxResult transmit(const CanMsg& msg);

    // Read TEC/REC counters and error flags. Clears rxOverflow as a side effect.
    CanError readAndClearErrors();

    // Returns true if TEC or REC >= 96, busOff, or rxOverflow — cheaper than readAndClearErrors().
    bool hasErrors();

    // Returns true if at least one frame is waiting in the RX FIFO.
    bool available();

    // Receive a frame. timeoutMs=0 (default): non-blocking. timeoutMs>0: blocks until frame or timeout.
    bool receive(CanMsg& msg, uint32_t timeoutMs = 0);

    // Configure an acceptance filter (index 0–31). All matched frames route to FIFO2.
    // ext=false matches standard frames; ext=true matches extended frames only.
    // Safe to call in normal mode.
    void setFilter(uint8_t index, uint32_t id, uint32_t mask, bool ext);

    // Disable a filter slot.
    void clearFilter(uint8_t index);

    // Restore catch-all filter on slot 0 and disable filters 1–31.
    void resetFilters();

    // Enter Configuration mode, halting TX/RX. Call restart() to resume.
    CanStatus stop();

    // Return to the mode active before stop().
    CanStatus restart();

    // Enter low-power Sleep mode. Wake-up via bus activity or wake().
    CanStatus sleep();

    // Exit Sleep mode and restore the mode active before sleep().
    CanStatus wake();

    // Return the current operating mode (compare against MODE_* constants).
    uint8_t getMode();

    // Return the detected oscillator frequency in Hz. Valid after configure().
    uint32_t getFsys() const { return mFsys; }

    // ------------------------------------------------------------------------
    // Raw / advanced API — direct register control for non-standard rates.
    // Presets for common rates are in mcp2518fd_presets.h.
    CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode);
    CanStatus configureRaw(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg, uint8_t mode, const CanConfig& cfg);
    CanStatus setDataBitTimingRaw(uint32_t dbtcfg, uint32_t tdcfg);

private:
    MCP2518SPI mSpi;
    uint32_t   mFsys          = 0;
    uint32_t   mTxTimeoutMs   = 10;
    uint32_t   mNbtcfg        = 0;
    int8_t     mIntPin        = NO_INT_PIN;
    bool       mTimestamp     = false;
    uint8_t    mStopPrevMode  = MODE_CONFIG;
    uint8_t    mSleepPrevMode = MODE_CONFIG;
    volatile bool mRxPending  = false;

    static MCP2518Driver* sIsrInstance;
    static void IRAM_ATTR sIsrHandler();

    uint32_t detectFsys();
    void configFifos(uint8_t rxFifoDepth, bool enableTimestamp);
    void configFilter();
    void applyTiming(uint32_t nbtcfg, uint32_t dbtcfg, uint32_t tdcfg);
    uint16_t txRamAddr();
    uint16_t rxRamAddr();
};

// Bit timing presets — see mcp2518fd_presets.h
#include "mcp2518fd_presets.h"
