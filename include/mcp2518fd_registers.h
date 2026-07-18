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

// TEF (Transmit Event FIFO)
constexpr uint16_t REG_CiTEFCON = 0x040;
constexpr uint16_t REG_CiTEFSTA = 0x044;
constexpr uint16_t REG_CiTEFUA  = 0x048;
// 0x04C reserved

// TX Queue
constexpr uint16_t REG_CiTXQCON = 0x050;
constexpr uint16_t REG_CiTXQSTA = 0x054;
constexpr uint16_t REG_CiTXQUA  = 0x058;

// FIFO registers — each FIFO occupies 0x0C bytes: CON, STA, UA
// FIFO m (m = 1..31): base = 0x05C + (m-1)*0x0C
constexpr uint16_t REG_CiFIFOCON1 = 0x05C;
constexpr uint16_t REG_CiFIFOSTA1 = 0x060;
constexpr uint16_t REG_CiFIFOUA1  = 0x064;

constexpr uint16_t REG_CiFIFOCON2 = 0x068;
constexpr uint16_t REG_CiFIFOSTA2 = 0x06C;
constexpr uint16_t REG_CiFIFOUA2  = 0x070;

// Helper: compute CON/STA/UA address for any FIFO m (1-based)
inline constexpr uint16_t FIFO_CON(uint8_t m) { return 0x05C + (m - 1) * 0x0C; }
inline constexpr uint16_t FIFO_STA(uint8_t m) { return 0x060 + (m - 1) * 0x0C; }
inline constexpr uint16_t FIFO_UA(uint8_t m)  { return 0x064 + (m - 1) * 0x0C; }

// Filter registers (DS20006027B page 13)
// CiFLTCONm: m=0..7, each register controls 4 filters
// byte 0 = Filter 0: FLTEN0 (bit7) + F0BP[4:0]
constexpr uint16_t REG_CiFLTCON0 = 0x1D0;
// CiFLTOBJm / CiMASKm: stride 8 bytes per filter (OBJ then MASK)
constexpr uint16_t REG_CiFLTOBJ0 = 0x1F0;
constexpr uint16_t REG_CiMASK0   = 0x1F4;

// RAM
constexpr uint16_t RAM_BASE = 0x400;

// CiFIFOCONm bit positions (Register 3-29, DS20006027B page 54)
// bits 31-29: PLSIZE[2:0]  bits 28-24: FSIZE[4:0]
// bits 22-21: TXAT[1:0]  bits 20-16: TXPRI[4:0]
// bit 10: FRESET  bit 9: TXREQ  bit 8: UINC
// bit 7: TXEN  bit 6: RTREN  bit 5: RXTSEN
constexpr uint32_t FIFOCON_PLSIZE_SHIFT = 29;
constexpr uint32_t FIFOCON_FSIZE_SHIFT  = 24;
constexpr uint32_t FIFOCON_TXAT_3       = (1u << 21);  // TXAT=01: 3 retransmission attempts
constexpr uint32_t FIFOCON_FRESET       = (1u << 10);
constexpr uint32_t FIFOCON_TXREQ        = (1u << 9);
constexpr uint32_t FIFOCON_UINC         = (1u << 8);
constexpr uint32_t FIFOCON_TXEN         = (1u << 7);
constexpr uint32_t FIFOCON_RTREN        = (1u << 6);
constexpr uint32_t FIFOCON_RXTSEN       = (1u << 5);

// PLSIZE values (DS20006027B page 54)
constexpr uint8_t PLSIZE_8  = 0;
constexpr uint8_t PLSIZE_12 = 1;
constexpr uint8_t PLSIZE_16 = 2;
constexpr uint8_t PLSIZE_20 = 3;
constexpr uint8_t PLSIZE_24 = 4;
constexpr uint8_t PLSIZE_32 = 5;
constexpr uint8_t PLSIZE_48 = 6;
constexpr uint8_t PLSIZE_64 = 7;

// CiFIFOSTAm bit positions (Register 3-30, DS20006027B page 57)
// bits 12-8: FIFOCI[4:0]
// bit 7: TXABT  bit 6: TXLARB  bit 5: TXERR
// bit 4: TXATIF  bit 3: RXOVIF
// bit 2: TFERFFIF  bit 1: TFHRFHIF  bit 0: TFNRFNIF
constexpr uint32_t FIFOSTA_TXABT    = (1u << 7);
constexpr uint32_t FIFOSTA_TXLARB   = (1u << 6);
constexpr uint32_t FIFOSTA_TXERR    = (1u << 5);
constexpr uint32_t FIFOSTA_TXATIF   = (1u << 4);
constexpr uint32_t FIFOSTA_RXOVIF   = (1u << 3);
constexpr uint32_t FIFOSTA_TFERFFIF = (1u << 2);
constexpr uint32_t FIFOSTA_TFHRFHIF = (1u << 1);
constexpr uint32_t FIFOSTA_TFNRFNIF = (1u << 0);

// CiCON byte 2 bits (bits 23:16)
constexpr uint8_t  CON2_RTXAT           = (1u << 0);  // bit 16 — restrict retransmission attempts
constexpr uint32_t CON_REQOP_SHIFT = 24;
constexpr uint32_t CON_OPMOD_SHIFT = 21;

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