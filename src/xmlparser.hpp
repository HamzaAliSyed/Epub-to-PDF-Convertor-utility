#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <stdexcept>
#include <format>
#include <span>
#include <ranges>
#include <concepts>
#include <expected>

class XMLNode;

using AttributePair = std::pair<std::string, std::string>;
using NodePointer = std::unique_ptr<XMLNode>;
using NodeSpan = std::span<const NodePointer>;

class XMLNode {
    private:

    std::string name;
    std::string content;
    std::vector<AttributePair> attributes;
    std::vector<NodePointer> childern;
    XMLNode* parent{nullptr};

    public:

    explicit XMLNode(std::string_view nodeName) : name(nodeName) {}
    
    auto addAttribute(std::string_view key, std::string_view value) -> void {
        attributes.emplace_back(std::string{key},std::string{value});
    }

    auto addChild(NodePointer child) -> void {
        child -> parent = this;
        childern.push_back(std::move(child));
    }

    [[nodiscard]] auto getName() const -> std::string_view {return name;}
    [[nodiscard]] auto getContent() const -> std::string_view {return content;}
    [[nodiscard]] auto getAttributes() const -> std::span<const AttributePair> {
        return std::span{attributes};
    }

    [[nodiscard]] auto findChildByName(std::string_view childName) const -> const XMLNode* {
        auto it = std::ranges::find_if(childern, [childName](const auto &child){
            return child->getName() == childName;
        });
        return it != childern.end() ?  it->get() : nullptr;
    }

    [[nodiscard]] auto getAttributeValue(std::string_view key) const -> std::optional<std::string_view> {
        auto it = std::ranges::find_if(attributes, [key](const auto &attribute){return attribute.first == key;});
        return it != attributes.end() ? std::optional{std::string_view{it->second}} : std::nullopt;
    }

    void setContent(std::string text) {
        content = std::move(text);
    }
};

class XMLParser {
    private:

    std::string_view content;
    size_t position{0};

    void skipWhiteSpace() {
        while (position < content.length() && std::isspace(content[position])) {
            position++;
        }
    }

    [[nodiscard]] char peek() const {
        return position < content.length() ? content[position] : '\0';
    }

    void advance() {
        if (position < content.length()) {
            position++;
        }
    }

    [[nodiscard]] bool match(char expected) {
        if (peek() == expected) {
            advance();
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string parseName() {
        skipWhiteSpace();
        std::string name;
        
        while (position < content.length() && 
               (std::isalnum(peek()) || peek() == '_' || peek() == '-' || peek() == ':')) {
            name += peek();
            advance();
        }
        
        return name;
    }

    [[nodiscard]] std::pair<std::string, std::string> parseAttribute() {
        skipWhiteSpace();
        std::string name = parseName();
        
        skipWhiteSpace();
        if (!match('=')) {
            throw std::runtime_error(std::format("Expected '=' at position {}", position));
        }

        skipWhiteSpace();
        if (!match('"')) {
            throw std::runtime_error(std::format("Expected '\"' at position {}", position));
        }

        std::string value;
        while (peek() != '"' && position < content.length()) {
            value += peek();
            advance();
        }

        if (!match('"')) {
            throw std::runtime_error(std::format("Expected closing '\"' at position {}", position));
        }

        return {name, value};
    }

    [[nodiscard]] std::unique_ptr<XMLNode> parseNode() {
        skipWhiteSpace();
        
        if (!match('<')) {
            throw std::runtime_error(std::format("Expected '<' at position {}", position));
        }

        std::string nodeName = parseName();
        if (nodeName.empty()) {
            throw std::runtime_error(std::format("Expected node name at position {}", position));
        }

        auto node = std::make_unique<XMLNode>(nodeName);

        skipWhiteSpace();
        while (peek() != '>' && peek() != '/' && position < content.length()) {
            auto [name, value] = parseAttribute();
            node->addAttribute(name, value);
            skipWhiteSpace();
        }

        if (match('/')) {
            if (!match('>')) {
                throw std::runtime_error(std::format("Expected '>' at position {}", position));
            }
            return node;
        }

        if (!match('>')) {
            throw std::runtime_error(std::format("Expected '>' at position {}", position));
        }

        while (position < content.length()) {
            skipWhiteSpace();
            
            if (match('<')) {
                if (match('/')) {  
                    std::string closingName = parseName();
                    if (closingName != nodeName) {
                        throw std::runtime_error(
                            std::format("Mismatched closing tag. Expected {} but got {} at position {}", 
                                      nodeName, closingName, position)
                        );
                    }
                    if (!match('>')) {
                        throw std::runtime_error(std::format("Expected '>' at position {}", position));
                    }
                    return node;
                } else {  
                    position--;  
                    node->addChild(parseNode());
                }
            } else {
                std::string text;
                while (peek() != '<' && position < content.length()) {
                    text += peek();
                    advance();
                }
                if (!text.empty() && !std::ranges::all_of(text, ::isspace)) {
                    auto textNode = std::make_unique<XMLNode>("#text");
                    textNode->setContent(std::move(text));
                    node->addChild(std::move(textNode));
                }
            }
        }

        throw std::runtime_error(std::format("Unexpected end of XML at position {}", position));
    }

    struct ParseError {
        std::string message;
        size_t position;
    };

    template <typename ParsedResult>
    using ParseResult = std::expected<ParsedResult,ParseError>;

    public:

    explicit XMLParser(std::string_view XMLContent) : content(XMLContent) {}

    XMLParser(const XMLParser&) = delete;
    XMLParser& operator=(const XMLParser&) = delete;
    XMLParser(XMLParser&&) noexcept = default;
    XMLParser& operator=(XMLParser&&) noexcept = default;

     [[nodiscard]] std::unique_ptr<XMLNode> parse() {
        try {
            skipWhiteSpace();
            if (position >= content.length()) {
                throw std::runtime_error("Empty XML content");
            }
            return parseNode();
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::format("XML parsing error at position {}: {}", position, e.what())
            );
        }
    }
};