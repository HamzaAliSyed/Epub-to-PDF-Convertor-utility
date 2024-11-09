#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <memory>

class XMLNode;
using XMLNodePointer = std::shared_ptr<XMLNode>;
using XMLNodeList = std::vector<XMLNodePointer>;
using XMLAttributes = std::unordered_map<std::string, std::string>;

enum class XMLNodeType {
    Element,
    Text,
    Comment,
    Document
};

class XMLNode : public std::enable_shared_from_this<XMLNode> {
    private:
        XMLNodeType type_;
        std::string name_;
        XMLAttributes attributes_;
        XMLNodeList children_;
        std::weak_ptr<XMLNode> parent_;

    public:
        explicit XMLNode(XMLNodeType type);
        
        auto GetType() const -> XMLNodeType;
        auto GetName() const -> std::string;
        auto SetName(const std::string& name) -> void;
        
        auto GetAttributes() -> XMLAttributes&;
        auto GetChildren() -> XMLNodeList&;
        
        auto AddChild(XMLNodePointer child) -> void;
        auto GetParent() const -> XMLNodePointer;
};

class XMLParseError : public std::runtime_error {
    public:
        explicit XMLParseError(const std::string& message);
        explicit XMLParseError(const char* message);
        const char* what() const noexcept override;
};

class UTF8Utility {
    public:
        static auto getUTF8SequenceLength(uint8_t firstByte) -> int;
        static auto isValidUTF8Sequence(uint8_t firstByte, const std::string_view::const_iterator& current,
            const std::string_view::const_iterator& end) -> bool;
        static auto isValidCodepoint(uint32_t codePoint) -> bool;
};

class UTF8Iterator {
    private:
        std::string_view::const_iterator current_;
        std::string_view::const_iterator end_;

    public:
        UTF8Iterator(std::string_view::const_iterator current, std::string_view::const_iterator end);
        auto CurrentCharacter() -> uint32_t;
        auto operator++() -> UTF8Iterator&;
        auto operator==(const UTF8Iterator& other) const -> bool;
        auto operator!=(const UTF8Iterator& other) const -> bool;
        auto underlying() const -> std::string_view::const_iterator;
};