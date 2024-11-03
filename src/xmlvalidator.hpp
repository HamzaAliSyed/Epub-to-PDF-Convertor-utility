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
                    std::optional<int> minLength;
                    std::optional<int> maxLength;
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
        };
};