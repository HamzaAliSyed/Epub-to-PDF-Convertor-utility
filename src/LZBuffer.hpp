#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <system_error>

template <typename Write>

class LZBuffer
{
public:
    virtual ~LZBuffer() = default;
    virtual std::size_t length() const = 0;
    virtual uint8_t lastOr(uint8_t literal) const = 0;
    virtual std::error_code lastN(std::size_t distance, uint8_t &output) const = 0;
    virtual std::error_code appendLiteral(uint8_t literal) = 0;
    virtual std::error_code appendLZ(std::size_t lengthVariable, std::size_t distance) = 0;
    virtual std::error_code finish(Write &output) = 0;
};

