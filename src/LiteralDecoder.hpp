#pragma once

#include <vector>
#include <cstdint>
#include "RangeDecoder.hpp"

class LiteralDecoder {
private:
    size_t numberOfContexts;
    size_t numberOfSymbols;
    std::vector<std::vector<uint16_t>> probabilities;

public:
    LiteralDecoder(size_t numberOfContexts, size_t numberOfSymbols)
        : numberOfContexts(numberOfContexts), numberOfSymbols(numberOfSymbols) {
        probabilities.resize(numberOfContexts, std::vector<uint16_t>(numberOfSymbols, 0x400));
    }

    uint8_t decodeLiteral(RangeDecoder<std::istream>& decoder, uint8_t prevByte) {
        size_t context = prevByte & (numberOfContexts - 1);
        uint32_t symbol = 1;
        
        for (size_t bit = 0; bit < 8; ++bit) {
            uint16_t& prob = probabilities[context][symbol];
            bool bitValue = decoder.decodeBit(prob, true);
            symbol = (symbol << 1) | bitValue;
        }
        
        return static_cast<uint8_t>(symbol);
    }
};
