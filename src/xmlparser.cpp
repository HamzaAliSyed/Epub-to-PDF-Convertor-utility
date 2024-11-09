#include "xmlparser.hpp"

XMLNode::XMLNode(XMLNodeType type) : type_(type) {}

auto XMLNode::GetType() const -> XMLNodeType {
    return type_;
}

auto XMLNode::GetName() const -> std::string {
    return name_;
}

auto XMLNode::SetName(const std::string& name) -> void {
    name_ = name;
}

auto XMLNode::GetAttributes() -> XMLAttributes& {
    return attributes_;
}

auto XMLNode::GetChildren() -> XMLNodeList& {
    return children_;
}

auto XMLNode::AddChild(XMLNodePointer child) -> void {
    children_.push_back(child);
    child->parent_ = shared_from_this();
}

auto XMLNode::GetParent() const -> XMLNodePointer {
    return parent_.lock();
}

XMLParseError::XMLParseError(const std::string& message) : std::runtime_error(message) {}
XMLParseError::XMLParseError(const char* message) : std::runtime_error(message) {}

const char* XMLParseError::what() const noexcept {
    return std::runtime_error::what();
}

auto UTF8Utility::getUTF8SequenceLength(uint8_t firstByte) -> int {
    if ((firstByte & 0x80) == 0) return 1;
    if ((firstByte & 0xE0) == 0xC0) return 2;
    if ((firstByte & 0xF0) == 0xE0) return 3;
    if ((firstByte & 0xF8) == 0xF0) return 4;
    return -1;
}

auto UTF8Utility::isValidUTF8Sequence(uint8_t firstByte, const std::string_view::const_iterator& current,
    const std::string_view::const_iterator& end) -> bool {
    int expectedLength = getUTF8SequenceLength(firstByte);
    if (expectedLength == -1) return false;
    if (std::distance(current, end) < expectedLength) return false;
    for(int index = 1; index < expectedLength; ++index) {
        uint8_t byte = static_cast<uint8_t>(*(current + index));
        if ((byte & 0xC0) != 0x80) return false;
    }
    return true;
}

auto UTF8Utility::isValidCodepoint(uint32_t codePoint) -> bool {
    return (codePoint <= 0x10FFFF) && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
}

// UTF8Iterator implementations
UTF8Iterator::UTF8Iterator(std::string_view::const_iterator current, std::string_view::const_iterator end) 
    : current_{current}, end_{end} {}

auto UTF8Iterator::CurrentCharacter() -> uint32_t {
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

auto UTF8Iterator::operator++() -> UTF8Iterator& {
    if (current_ >= end_) return *this;

    uint8_t firstByte = static_cast<uint8_t>(*current_);

    if (firstByte < 0x80) current_++;
    else if ((firstByte & 0xE0) == 0xC0) current_ += 2;
    else if ((firstByte & 0xF0) == 0xE0) current_ += 3;
    else if ((firstByte & 0xF8) == 0xF0) current_ += 4;
    else throw XMLParseError("Invalid UTF-8 sequence");

    return *this;
}

auto UTF8Iterator::operator==(const UTF8Iterator& other) const -> bool {
    return current_ == other.current_;
}

auto UTF8Iterator::operator!=(const UTF8Iterator& other) const -> bool {
    return !(*this == other);
}

auto UTF8Iterator::underlying() const -> std::string_view::const_iterator {
    return current_;
}