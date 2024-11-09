#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <memory>

class XMLNode;
class XMLDocument;

class XMLParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class UTF8Iterator {
    private:
        std::string_view::const_iterator current_;
        std::string_view::const_iterator end_;

    public:
        UTF8Iterator(std::string_view::const_iterator current, std::string_view::const_iterator end) 
            : current_{current}, end_{end} {}
        
        auto CurrentCharacter() -> uint32_t {
            if (current_ >= end_) return 0;

            uint32_t codePoint = 0;
            uint8_t firstByte = static_cast<uint8_t>(*current_);

            if (firstByte < 0x80) {
                return firstByte;
            }

            int numberOfBytes;

            if ((firstByte & 0xE0) == 0xC0) numberOfBytes = 2;
            else if ((firstByte & 0xF0) == 0xE0) numberOfBytes = 3;
            else if ((firstByte & 0xF8) == 0xF0) numberOfBytes = 4;
            else throw XMLParseError("Invalid UTF-8 sequence");

            switch(numberOfBytes) {
                case 2: {
                    codePoint = (firstByte & 0x1F) << 6;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 1)) & 0x3F);
                    break;
                }
                case 3: {
                    codePoint = (firstByte & 0x0F) << 12;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 1)) & 0x3F) << 6;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 2)) & 0x3F);
                    break;
                }
                case 4: {
                    codePoint = (firstByte & 0x07) << 18;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 1)) & 0x3F) << 12;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 2)) & 0x3F) << 6;
                    codePoint |= (static_cast<uint8_t>(*(current_ + 3)) & 0x3F);
                    break;
                }
            }

            return codePoint;
        }
};