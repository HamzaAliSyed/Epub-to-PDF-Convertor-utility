#pragma once

#include "xmlparser.hpp"
#include <map>
#include <regex>
#include <optional>
#include <variant>
#include <string_view>
#include <algorithm>

class XMLValidator {
    private:
        std::map<std::string, std::pair<std::string,std::string>> namespaces;

        const std::map<std::string,std::string> specialCharacters = {
            {"&lt;", "<"},
            {"&gt;", ">"},
            {"&amp;", "&"},
            {"&quot;", "\""},
            {"&apos;", "'"}
        };

    public:
        class XSDType {
            public:
                enum class BaseType {
                    String,
                    Integer,
                    Decimal,
                    Boolean,
                    Date,
                    DateTime
                };

                struct Constraint {
                    std::optional<std::string> pattern;
                    std::optional<std::string> enumeration;
                    std::optional<size_t> minLength;
                    std::optional<size_t> maxLength;
                    std::optional<std::string> minValue;
                    std::optional<std::string> maxValue;
                };

                BaseType baseType;
                Constraint constraints;

            private:
                [[nodiscard]] auto ValidateString(const std::string& value) const -> bool {
                    if (constraints.pattern) {
                        try {
                            std::regex pattern(*constraints.pattern);
                            if (!std::regex_match(value,pattern)) return false;
                        } catch (...) {
                            return false;
                        }
                    }

                    if (constraints.minLength && value.length() < *constraints.minLength) return false;
                    if (constraints.maxLength && value.length() > *constraints.maxLength) return false;

                    if (constraints.enumeration) {
                        return *constraints.enumeration == value;
                    }

                    return true;
                }

                [[nodiscard]] static auto ValidateInteger(const std::string& value) -> bool {
                    static const std::regex integerRegex("^-?\\d+$");
                    return std::regex_match(value,integerRegex);
                }

                [[nodiscard]] static auto ValidateBoolean(const std::string& value) -> bool {
                    return value == "true" || value == "false" || value == "0" || value == "1";
                }

                [[nodiscard]] static auto ValidateDecimal(const std::string& value) -> bool {
                    static const std::regex decimalRegex("^-?\\d*\\.?\\d+$");
                    return std::regex_match(value, decimalRegex);
                }

                [[nodiscard]] static auto ValidateDateTime(const std::string& value) -> bool {
                    static const std::regex 
                    dateTimeRegex("^\\d{4}-\\d{2}-\\d{2}(T\\d{2}:\\d{2}:\\d{2}(Z|[+-]\\d{2}:\\d{2})?)?$");
                    return std::regex_match(value, dateTimeRegex);
                }

            public:
                [[nodiscard]] auto Validate(const std::string& value) const -> bool {
                    switch(baseType) {
                        case BaseType::Integer: {
                            return ValidateInteger(value);
                        }
                        case BaseType::String: {
                            return ValidateString(value);
                        }
                        case BaseType::Decimal: {
                            return ValidateDecimal(value);
                        }
                        case BaseType::Boolean: {
                            return ValidateBoolean(value);
                        }
                        case BaseType::DateTime: {
                            return ValidateDateTime(value);
                        }
                        default: {
                            return false;
                        }
                    }
                }
        };
        class XPath {
            private:
                std::string expressions;

                struct Token {
                    enum class Type {
                        Element,
                        Attribute,
                        Wildcard,
                        Parent,
                        Self,
                        Predicate
                    } type;
                    std::string value;
                };

                [[nodiscard]] auto Tokenize() const -> std::vector<Token> {
                    std::vector<Token> tokens;
                    std::string current;

                    for (size_t index = 0; index < expressions.length(); ++index) {
                        char letter = expressions[index];

                        switch(letter) {
                            case '/' : {
                                if (!current.empty()) {
                                    tokens.push_back({Token::Type::Element, std::move(current)});
                                    current.clear();
                                }
                                break;
                            }
                            case '@' : {
                                if (!current.empty()) {
                                    tokens.push_back({Token::Type::Element, std::move(current)});
                                    current.clear();
                                }
                                tokens.push_back({Token::Type::Attribute, ""});
                                break;
                            }
                            case '*' : {
                                tokens.push_back({Token::Type::Wildcard, "*"});
                                break;
                            }
                            case '[' : {
                                if (index >= expressions.length()) {
                                    break;
                                }
                                while (index < expressions.length() && expressions[index] != ']') {
                                    current += expressions[++index];
                                }
                                tokens.push_back({Token::Type::Predicate,std::move(current)});
                                current.clear();
                                break;
                            }
                            default : {
                                current += letter;
                            }
                        }
                    }

                    if (!current.empty()) {
                        tokens.push_back({Token::Type::Element, std::move(current)});
                    }

                    return tokens;
                }
            public:
                explicit XPath(std::string expressionInput) : expressions(expressionInput) {}

                [[nodiscard]] auto Evaluate(const XMLNode& root) const -> std::vector<const XMLNode*> {
                    std::vector<const XMLNode*> results;
                    auto tokens = Tokenize();

                    std::function<void(const XMLNode*,size_t)> traverse = [&] (const XMLNode* node, size_t tokenIndex) 
                    {
                        if (tokenIndex >= tokens.size()) {
                            results.push_back(node);
                            return;
                        }

                        const auto& token = tokens[tokenIndex];

                        switch (token.type) {
                            case Token::Type::Element : {
                                for (const auto& child : node->GetChildren()) {
                                    if (child -> GetName() == token.value) {
                                        traverse(child.get(), tokenIndex + 1);
                                    }
                                }
                                break;
                            }
                            case Token::Type::Wildcard : {
                                for (const auto& child : node->GetChildren()) {
                                    traverse(child.get(), tokenIndex+1);
                                }
                                break;
                            }
                            case Token::Type::Attribute : {
                                break;
                            }
                            default : {
                                break;
                            }
                        }
                    };

                    traverse(&root,0);
                    return results;
                }
        };

        class DTDValidator {
            public:
                struct ElementRule {
                    enum class Occurrence {
                        One,
                        ZeroOrOne,
                        ZeroOrMore,
                        OneOrMore
                    };

                    std::string elementName;
                    std::vector<std::pair<std::string, Occurrence>> childElements;
                    bool allowText;
                };

            private:
                std::map<std::string, ElementRule> elementRules;
                std::map<std::string, std::string> entities;

            public:
                auto AddElementRule(ElementRule rule) -> void {
                    elementRules[rule.elementName] = std::move(rule);
                }

                auto AddEntity(std::string name, std::string value) -> void {
                    entities[std::move(name)] = std::move(value);
                }

                [[nodiscard]] auto ValidateNode(const XMLNode& node) const -> bool {
                    auto it = elementRules.find(node.GetName());

                    if (it == elementRules.end()) {
                        return false;
                    }

                    const auto& rule = it -> second;

                    for (const auto& [childName, occurrence] : rule.childElements) {
                        size_t count = std::count_if(
                            node.GetChildren().begin(),
                            node.GetChildren().end(),
                            [&](const auto& child) {return child->GetName() == childName;}
                        );

                        switch (occurrence) {
                            case ElementRule::Occurrence::One : {
                                if (count != 1) {
                                    return false;
                                }
                                break;
                            }
                            case ElementRule::Occurrence::ZeroOrOne : {
                                if (count > 1) {
                                    return false;
                                }
                                break;
                            }
                            case ElementRule::Occurrence::OneOrMore : {
                                if (count < 1) {
                                    return false;
                                }
                                break;
                            }
                            case ElementRule::Occurrence::ZeroOrMore : {
                                break;
                            }
                        }
                    }

                    return true;
                }
        };

        [[nodiscard]] auto EncodeSpecialCharacter(std::string_view text) const -> std::string {
            std::string result;
            result.reserve(text.length());

            for (char character : text) {
                switch (character) {
                    case '<' : result += "&lt;"; break;
                    case '>' : result += "&gt;"; break;
                    case '&' : result += "&amp;"; break;
                    case '"' : result += "&quot;"; break;
                    case '\'' : result += "&apos;"; break;
                    default: result += character;
                }
            }

            return result;
        }

        [[nodiscard]] auto DecodeSpecialCharacter(std::string_view text) const -> std::string {
            std::string result{text};
            for (const auto& [encoded, decoded] : specialCharacters) {
                size_t position = 0;
                while((position = result.find(encoded,position)) != std::string::npos) {
                    result.replace(position, encoded.length(), decoded);
                    position += decoded.length();
                }
            }
            return result;
        }

        auto RegisterNamespace(std::string prefix, std::string uri, std::string schema) -> void {
            if (prefix.empty() || uri.empty()) {
                throw std::invalid_argument("Namespace prefix and URI cannot be empty");
            }
            namespaces[std::move(prefix)] = {std::move(uri), std::move(schema)};
        }

        [[nodiscard]] auto ResolveNamespace(const std::string& prefix) const 
        -> std::optional<std::pair<std::string, std::string>> {
            auto it = namespaces.find(prefix);
            if (it != namespaces.end()) {
                return it->second;
            }
            return std::nullopt;
        }
};