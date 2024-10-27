#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include <istream>
#include <cstdint>
class LZMADecryptor {
    bool ReadTag(std::istream& input, const std::vector<uint8_t>& tag);
};