#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <cstdint>
#include "minorutility.hpp"

template <typename ReadStream>
class RangeDecoder {
    public:
    ReadStream *stream;
    uint32_t range;
    uint32_t code;

    RangeDecoder(ReadStream &inputStream) : stream(&inputStream), range(0xFFFFFFFF), code(0) {
        uint8_t discard;
        stream -> read(reinterpret_cast<char*>(&discard), 1);
        stream -> read(reinterpret_cast<char*>(&code), sizeof(code));
        code = __builtin_bswap32(code);
    }

    RangeDecoder(ReadStream &inputStream, uint32_t initRange, uint32_t initCode) :
    stream(&inputStream), range(initRange), code(initCode) {}

    void set(uint32_t newRange, uint32_t newCode) {
        range = newRange;
        code = newCode;
    }

    bool isFinishedOK() const {
        return code == 0 || MinorUtility::IsEndOfFile(*stream);
    }

    void normalize() {
        if (range < 0x01000000) {
            range <<=8;
            uint8_t nextByte;
            stream -> read(reinterpret_cast<char*>(&nextByte), 1);
            code = (code  << 8) ^ nextByte;
        }
    }

    bool getBit() {
        range >>= 1;
        bool bit = code >= range;

        if (bit) {
            code -= range;
        }

        normalize();
        return bit;
    }

    uint32_t get(size_t count) {
        uint32_t result = 0;
        for (size_t index = 0; index < count; index++) {
            result = (result << 1) | static_cast<uint32_t>(getBit());
        }
        return result;
    }

    bool decodeBit(uint16_t &probability, bool update) {
        uint32_t bound = (range >> 11) * probability;

        bool bit = code >= bound;
        if (bit) {
            code -= bound;
            range -= bound;
            if (update) {
                probability -= probability >> 5;
            }
        } else {
            range = bound;
            if (update) {
                probability += (0x800 - probability) >> 5;
            }
        }

        normalize();
        return bit;
    }

    uint32_t parseBitTree(size_t numberOfBits, std::vector<uint16_t> &probabilities, bool update) {
        uint32_t temp = 1;
        for (size_t index = 0; index < numberOfBits; index++) {
            bool bit = decodeBit(probabilities[temp], update);
            temp = (temp << 1) | bit;
        }
        return temp - (1 << numberOfBits);
    }

    uint32_t parseReverseBitTree(size_t numberOfBits, std::vector<uint16_t> &probabilities, size_t offset, bool update) {
        uint32_t result = 0;
        uint32_t temp = 1;

        for (size_t index = 0; index < numberOfBits; index++) {
            bool bit = decodeBit(probabilities[offset + temp], update);
            temp = (temp << 1) | bit;
            result |= static_cast<uint32_t>(bit) << index;
        }
        return result;
    }
};