#include "crc.hpp"

CRC::CRC(uint32_t poly, uint32_t init) noexcept : polynomial(poly), initialValue(init), currentCRC(init) {
    generateTable();
}

void CRC::generateTable() noexcept {
    for (uint32_t index = 0; index < 256; ++ index) {
        uint32_t crc = index << 24;

        for (uint32_t secondIndex = 0; secondIndex < 8; ++secondIndex) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<=1;
            }
        }

        crcTable[index] = crc;
    }
}

uint32_t CRC::updateByte(uint32_t crc, uint8_t data) const noexcept {
    uint8_t tableIndex = (crc >> 24) ^ data;
    return (crc << 8) ^ crcTable[tableIndex];
}

uint32_t CRC::calculate(std::span<const uint8_t> data) const noexcept {
    uint32_t crc = initialValue;

    for (uint8_t byte: data) {
        crc = updateByte(crc,byte);
    }

    return crc ^ FINALXOR;
}

uint32_t CRC::finalize() const noexcept {
    return currentCRC ^ FINALXOR;
}

void CRC::update(std::span<const uint8_t> data) noexcept {
    for (uint8_t byte: data) {
        currentCRC = updateByte(currentCRC,byte);
    }
}

void CRC::reset() noexcept {
    currentCRC = initialValue;
}