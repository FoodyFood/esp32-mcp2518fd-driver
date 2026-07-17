#include "mcp2518fd_spi.h"
#include "mcp2518fd_registers.h"

MCP2518SPI::MCP2518SPI(SPIClass& spi, uint8_t csPin)
    : mSPI(spi),
      mCS(csPin)
{
}

void MCP2518SPI::begin()
{
    pinMode(mCS, OUTPUT);
    deselect();
}

void MCP2518SPI::select()
{
    digitalWrite(mCS, LOW);
}

void MCP2518SPI::deselect()
{
    digitalWrite(mCS, HIGH);
}

void MCP2518SPI::sendInstruction(uint8_t command, uint16_t address)
{
    uint8_t first = command | ((address >> 8) & 0x0F);
    uint8_t second = address & 0xFF;

    mSPI.transfer(first);
    mSPI.transfer(second);
}

void MCP2518SPI::reset()
{
    select();
    mSPI.transfer(MCP_CMD_RESET);
    deselect();

    delay(10);
}

uint8_t MCP2518SPI::read8(uint16_t address)
{
    select();

    sendInstruction(MCP_CMD_READ, address);

    uint8_t value = mSPI.transfer(0x00);

    deselect();

    return value;
}

uint16_t MCP2518SPI::read16(uint16_t address)
{
    select();

    sendInstruction(MCP_CMD_READ, address);

    uint16_t value = mSPI.transfer(0x00);
    value |= (uint16_t)mSPI.transfer(0x00) << 8;

    deselect();

    return value;
}

uint32_t MCP2518SPI::read32(uint16_t address)
{
    select();

    sendInstruction(MCP_CMD_READ, address);

    uint32_t value = 0;

    value |= (uint32_t)mSPI.transfer(0x00);
    value |= (uint32_t)mSPI.transfer(0x00) << 8;
    value |= (uint32_t)mSPI.transfer(0x00) << 16;
    value |= (uint32_t)mSPI.transfer(0x00) << 24;

    deselect();

    return value;
}

void MCP2518SPI::write8(uint16_t address, uint8_t value)
{
    select();

    sendInstruction(MCP_CMD_WRITE, address);

    mSPI.transfer(value);

    deselect();
}

void MCP2518SPI::write32(uint16_t address, uint32_t value)
{
    select();

    sendInstruction(MCP_CMD_WRITE, address);

    mSPI.transfer((uint8_t)(value));
    mSPI.transfer((uint8_t)(value >> 8));
    mSPI.transfer((uint8_t)(value >> 16));
    mSPI.transfer((uint8_t)(value >> 24));

    deselect();
}

bool MCP2518SPI::setMode(uint8_t mode)
{
    // REQOP occupies bits 26:24, which are bits 2:0 of byte 3.
    // Write only that byte, leaving the remainder of CiCON untouched.

    uint8_t upper = read8(REG_CiCON + 3);

    upper &= 0xF8;            // Clear REQOP bits
    upper |= (mode & 0x07);   // Set requested mode

    write8(REG_CiCON + 3, upper);

    uint32_t start = millis();

    while ((millis() - start) < 100)
    {
        if (getMode() == mode)
            return true;

        delay(1);
    }

    return false;
}

uint8_t MCP2518SPI::getMode()
{
    // OPMOD occupies bits 23:21, which are bits 7:5 of byte 2.

    uint8_t byte2 = read8(REG_CiCON + 2);

    return (byte2 >> 5) & 0x07;
}
