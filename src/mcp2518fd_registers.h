#pragma once

#include <Arduino.h>

// SPI Instructions
constexpr uint8_t MCP_CMD_RESET = 0x00;
constexpr uint8_t MCP_CMD_READ  = 0x30;
constexpr uint8_t MCP_CMD_WRITE = 0x20;

// SFR addresses
constexpr uint16_t REG_OSC      = 0xE00;
constexpr uint16_t REG_IOCON    = 0xE04;
constexpr uint16_t REG_CRC      = 0xE08;
constexpr uint16_t REG_ECCCON   = 0xE0C;
constexpr uint16_t REG_ECCSTAT  = 0xE10;
constexpr uint16_t REG_DEVID    = 0xE14;

constexpr uint16_t REG_CiCON    = 0x000;
constexpr uint16_t REG_CiNBTCFG = 0x004;
constexpr uint16_t REG_CiDBTCFG = 0x008;
constexpr uint16_t REG_CiTDC    = 0x00C;
constexpr uint16_t REG_CiTBC    = 0x010;
constexpr uint16_t REG_CiTSCON  = 0x014;
constexpr uint16_t REG_CiVEC    = 0x018;
constexpr uint16_t REG_CiINT    = 0x01C;
constexpr uint16_t REG_CiRXIF   = 0x020;
constexpr uint16_t REG_CiTXIF   = 0x024;
constexpr uint16_t REG_CiRXOVIF = 0x028;
constexpr uint16_t REG_CiTXATIF = 0x02C;
constexpr uint16_t REG_CiTXREQ  = 0x030;
constexpr uint16_t REG_CiTREC   = 0x034;
constexpr uint16_t REG_CiBDIAG0 = 0x038;
constexpr uint16_t REG_CiBDIAG1 = 0x03C;

// FIFO base
constexpr uint16_t REG_CiFIFOCON1 = 0x05C;
constexpr uint16_t REG_CiFIFOSTA1 = 0x060;
constexpr uint16_t REG_CiFIFOUA1  = 0x064;

// TX Queue
constexpr uint16_t REG_CiTXQCON = 0x050;
constexpr uint16_t REG_CiTXQSTA = 0x054;
constexpr uint16_t REG_CiTXQUA  = 0x058;

// RAM
constexpr uint16_t RAM_BASE = 0x400;

// CiCON mode bits
constexpr uint32_t CON_REQOP_SHIFT = 21;
constexpr uint32_t CON_OPMOD_SHIFT = 24;

constexpr uint32_t CON_REQOP_MASK = 0x7u << CON_REQOP_SHIFT;
constexpr uint32_t CON_OPMOD_MASK = 0x7u << CON_OPMOD_SHIFT;

// Operating modes
constexpr uint8_t MODE_NORMAL      = 0;
constexpr uint8_t MODE_SLEEP       = 1;
constexpr uint8_t MODE_INTERNAL_LB = 2;
constexpr uint8_t MODE_LISTEN      = 3;
constexpr uint8_t MODE_CONFIG      = 4;
constexpr uint8_t MODE_EXTERNAL_LB = 5;
constexpr uint8_t MODE_CLASSIC     = 6;
constexpr uint8_t MODE_RESTRICTED  = 7;