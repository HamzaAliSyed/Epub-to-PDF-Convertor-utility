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

XMLParser::XMLParser(std::string_view content) 
    : content_(content), current_(content_.begin(), content_.end()), end_(content_.end(), content_.end()) 
    {}

auto XMLParser::Parse() -> XMLNodePointer {
    auto document = std::make_shared<XMLNode>(XMLNodeType::Document);

    SkipWhiteSpace();
    if (current_ != end_) {
        auto root = ParseElement();
        if (root) {
            document->AddChild(root);
        }
    }

    return document;
}

auto XMLParser::ParseElement()->XMLNodePointer {
    if (current_.CurrentCharacter() != '<') {
        throw XMLParseError("Expected '<' at start of element");
    }
    ++current_;

    auto element = std::make_shared<XMLNode>(XMLNodeType::Element);
    SkipWhiteSpace();
    element->SetName(ParseName());

    while (current_ != end_ ) {
        SkipWhiteSpace();

        uint32_t currentChar = current_.CurrentCharacter();
        if (currentChar == '>' || currentChar == '/') {
            break;
        }

        std::string attributeName = ParseName();

        if (attributeName.empty()) {
            break;
        }

        SkipWhiteSpace();

        ++current_;

        SkipWhiteSpace();

        uint32_t quotationMark = static_cast<uint32_t>(current_.CurrentCharacter());
        if (quotationMark != '"' && quotationMark != '\'') {
            throw XMLParseError("Expected quote for attribute value");
        }
        ++current_;

        std::string attributeValue;
        while (current_ != end_ && current_.CurrentCharacter() != quotationMark) {
            attributeValue += static_cast<char>(current_.CurrentCharacter());
            ++current_;
        }

        if (current_ == end_) {
            throw XMLParseError("Unterminated attribute value");
        }

        ++current_;

        element->GetAttributes()[attributeName] = attributeValue;
    }

    if (current_ == end_) {
        throw XMLParseError("Unexpected end of document");
    }

    if (current_.CurrentCharacter() == '/') {
        ++current_;
        if (current_.CurrentCharacter() != '>') {
            throw XMLParseError("Expected '>' after '/'");
        }
        ++current_;
        return element;
    }

    if (current_.CurrentCharacter() == '>') {
        ++current_;
    }

    return element;
}

auto XMLParser::ParseName()->std::string {
    std::string name;

    SkipWhiteSpace();

    while (current_ != end_) {
        uint32_t character = current_.CurrentCharacter();
        if (character == '>' || character == '/' || character == ' ' 
            || character == '\t' || character == '\n' || character == '\r' || character == '=') {
                break;
        }

        name += static_cast<char>(character);
        ++current_;
    }
    return name;
}

auto XMLParser::SkipWhiteSpace()->void {
    while (current_ != end_) {
        uint32_t character = current_.CurrentCharacter();
        if (character != ' ' && character != '\t' && character != '\n' && character != '\r') {
            break;
        }
        ++current_;
    }
}