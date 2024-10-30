#pragma once
#include "LZBuffer.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

template <typename Write>
class LZCircularBuffer : public LZBuffer<Write> {
    private:

    Write &stream;
    std::vector<uint8_t> buffer;
    std::size_t dictionarySize;
    std::size_t memoryLimit;
    std::size_t cursor;
    std::size_t lengthVariable;

    public:

    LZCircularBuffer(Write &outputStream, std::size_t dictionarySize,  std::size_t memoryLimit) :
    stream(outputStream), dictionarySize(dictionarySize), memoryLimit(memoryLimit),
          cursor(0), lengthVariable(0) {
        buffer.resize(dictionarySize, 0);
    }

    std::size_t length() const override {
        return lengthVariable;
    }

    uint8_t lastOr(uint8_t literal) const override {
        if (lengthVariable == 0) {
            return literal;
        }
        return buffer[(dictionarySize + cursor - 1) % dictionarySize];
    }

    std::error_code lastN(std::size_t distance, uint8_t &output) const override {
        if (distance > dictionarySize || distance > lengthVariable) {
            return std::make_error_code(std::errc::result_out_of_range);
        }
        auto offset = (dictionarySize + cursor - distance) % dictionarySize;
        output = buffer[offset];
        return {};
    }

    std::error_code appendLiteral(uint8_t literal) override {
        if (lengthVariable >= memoryLimit) {
            return std::make_error_code(std::errc::not_enough_memory);
        }

        buffer[cursor] = literal;
        cursor = (cursor + 1) % dictionarySize;
        lengthVariable++;

        if (cursor == 0) {
            stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            stream.flush();
        }

        return {};
    }

    std::error_code appendLZ(std::size_t sequenceLength, std::size_t distance) override {
        if (distance > dictionarySize || distance > lengthVariable) {
            return std::make_error_code(std::errc::result_out_of_range);
        }

        auto offset = (dictionarySize + cursor - distance) % dictionarySize;
        for (std::size_t index = 0; index < sequenceLength; index++) {
            buffer[cursor] = buffer[offset];
            cursor = (cursor + 1) % dictionarySize;
            offset = (offset + 1) % dictionarySize;
            lengthVariable++;

            if (cursor == 0) {
                stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                stream.flush();
            }
        }

        return {};
    }

    std::error_code finish(Write &output) override {
        if (cursor > 0) {
            output.write(reinterpret_cast<const char*>(buffer.data()), cursor);
            output.flush();
        }
        return {};
    }
};