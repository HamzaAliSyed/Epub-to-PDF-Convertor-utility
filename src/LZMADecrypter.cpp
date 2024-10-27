#include "LZMADecrypter.hpp"

bool LZMADecryptor::ReadTag(std::istream& input, const std::vector<uint8_t>& tag) {
    std::vector<uint8_t> buffer(tag.size(), 0);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (!input) {
        return false;
    }
    return std::memcmp(buffer.data(), tag.data(), tag.size()) == 0;
}