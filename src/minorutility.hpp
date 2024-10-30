#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include <istream>
#include <cstdint>

class MinorUtility {
    public:
    
    bool ReadTag(std::istream& input, const std::vector<uint8_t>& tag);
    bool IsEndOfFile(std::istream& input);
    bool FlushZeroPadding(std::istream& input);
    
};