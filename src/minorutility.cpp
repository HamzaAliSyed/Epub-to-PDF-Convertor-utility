#include "minorutility.hpp"

bool MinorUtility::ReadTag(std::istream& input, const std::vector<uint8_t>& tag) {
    std::vector<uint8_t> buffer(tag.size(), 0);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (!input) {
        return false;
    }
    return std::memcmp(buffer.data(), tag.data(), tag.size()) == 0;
}

bool MinorUtility::IsEndOfFile(std::istream& input) {
    return input.peek() == std::istream::traits_type::eof();
}

bool MinorUtility::FlushZeroPadding(std::istream& input) {
    std::vector<uint8_t> buffer(1024);
    while (true) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize length = input.gcount();
        if (length == 0) {
            return true;
        }
        for (std::streamsize index = 0; index < length; ++index) {
            if (buffer[index] != 0) {
                return false;
            }
        }
    }
}