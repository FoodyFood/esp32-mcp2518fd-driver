#pragma once

#include <Arduino.h>
#include <SPI.h>

class MCP2518SPI
{
public:
    MCP2518SPI(SPIClass& spi, uint8_t csPin);

    void begin();
    void reset();

    uint8_t  read8(uint16_t address);
    uint16_t read16(uint16_t address);
    uint32_t read32(uint16_t address);

    void write8(uint16_t address, uint8_t value);
    void write32(uint16_t address, uint32_t value);

    bool setMode(uint8_t mode);
    uint8_t getMode();

private:
    SPIClass& mSPI;
    uint8_t mCS;

    void select();
    void deselect();

    void sendInstruction(uint8_t command, uint16_t address);
};