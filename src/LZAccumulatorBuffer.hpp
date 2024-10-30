#pragma once
#include "LZBuffer.hpp"
#include <algorithm>
#include <stdexcept>

template <typename Write>
class LZAccumulatorBuffer : public LZBuffer<Write> {

    private:

    Write &stream;
    std::vector<uint8_t> buffer;
    std::size_t memoryLimit;
    std::size_t lengthVariable;

    public:

    LZAccumulatorBuffer(Write &outputStream, std::size_t memoryLimit) : stream(outputStream), memoryLimit(memoryLimit), lengthVariable(0) {}

    std::size_t length() const override {
        return lengthVariable;
    }

    std::error_code lastN(std::size_t distance, uint8_t &output) const override {
        if (distance > buffer.size()) {
            return std::make_error_code(std::errc::result_out_of_range);
        }

        output = buffer[buffer.size() - distance];
        return {};
    }

    std::error_code appendLiteral(uint8_t literal) override {
        if (lengthVariable + 1 > memoryLimit) {
            return std::make_error_code(std::errc::not_enough_memory);
        }
        buffer.push_back(literal);
        lengthVariable++;
        return {};
    }

    std::error_code appendLZ(std::size_t sequenceLength, std::size_t distance) override {
        if (distance > buffer.size()) {
            return std::make_error_code(std::errc::result_out_of_range);
        }
        auto offset = buffer.size() - distance;
        for (std::size_t index = 0; index < sequenceLength; index++) {
            buffer.push_back(buffer[offset + index]);
            lengthVariable++;
        }
        return {};
    }

    std::error_code finish(Write &output) override {
        output.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        output.flush();
        return {};
    }
};