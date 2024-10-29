#pragma once

#include <cstdint>
#include <span>
#include <array>
#include <vector>

class CRC {
    private:

    static constexpr uint32_t DEFAULT_POLYNOMIAL = 0x1EDC6F41;
    static constexpr uint32_t DEFAULT_INITIAL = 0xFFFFFFFF;

    std::array<uint32_t, 256> crcTable{};
    uint32_t polynomial;
    uint32_t initialValue;

    void generateTable() noexcept;

    public:

    explicit CRC (uint32_t poly = DEFAULT_POLYNOMIAL, uint32_t init = DEFAULT_INITIAL) noexcept;
    [[nodiscard]] uint32_t updateByte(uint32_t crc, uint8_t data) const noexcept;
    [[nodiscard]] uint32_t calculate(std::span<const uint8_t> data) const noexcept;
};