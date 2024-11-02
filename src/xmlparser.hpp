#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <algorithm>

using AttributePair = std::pair<std::string,std::string>;

class XMLNode {
    private:
        std::string name;
        std::string content;
        std::vector<AttributePair> attributes;
        std::vector<std::unique_ptr<XMLNode>> children;
    
    public:
        explicit XMLNode(std::string nodeName) : name(std::move(nodeName)) {}

        auto GetName() const noexcept -> const std::string& {return name;}
        auto GetContent() const noexcept -> const std::string& {return content;}
        auto SetContent(std::string nodeContent) noexcept -> void {content = std::move(nodeContent);}

        auto AddAttribute(std::string key, std::string value) -> void {
            attributes.emplace_back(std::move(key),std::move(value));
        }

        auto GetAttributeValue(std::string key) const -> std::string_view {
            for (const auto& [attrKey,attrValue] : attributes) {
                if (attrKey == key) {
                    return attrValue;
                }
            }

            return {};
        }

        auto AddChild(std::unique_ptr<XMLNode> child) -> void {
            children.push_back(std::move(child));
        }

        auto GetChildren() const -> const std::vector<std::unique_ptr<XMLNode>>& {
            return children;
        }

        auto FindChildByName(std::string_view childName) const -> const XMLNode* {
            for (const auto& child : children) {
                if (child -> GetName() == childName) {
                    return child.get();
                }
            }
            
            return nullptr;
        }

        auto GetAttributes() const -> const std::vector<AttributePair>& {
            return attributes;
        }

        auto ToString(size_t indent = 0) const -> std::string {
            std::string result(indent, ' ');
            result += '<' + name;

            for (const auto& [key,value] : attributes) {
                result += ' ' + key + "=\"" + value + '"';
            }

            if (children.empty() && content.empty()) {
                result += "/>\n";
                return result;
            }

            result += '>';

            if (!children.empty()) {
                result += '\n';
                for (const auto& child : children) {
                    result += child -> ToString(indent + 2);
                }
                result += std::string(indent,' ');
            } else if (!content.empty()) {
                result += content;
            }

            result += "</" + name + ">\n";
            return result;
        }
};

class XMLParseError {
    public:
        enum class ErrorType {
            None,
            InvalidTag,
            UnexpectedEOF,
            InvalidAttribute,
            MalformedXML,
            UnclosedTag,
            InvalidCDATA,
            InvalidComment
        };

    private:
        ErrorType errorType;
        size_t position;
        std::string message;

    public:
        XMLParseError(ErrorType type, size_t pos, std::string msg) 
        : errorType(type), position(pos), message(std::move(msg)) {}

        auto GetType() const -> ErrorType {return errorType;}
        auto GetPosition() const -> size_t {return position;}
        auto GetMessage() const -> const std::string& {return message;}
};

class XMLParser {
    private:
        std::string_view content;
        size_t position{0};
        std::optional<XMLParseError> lastError;

        auto SetError(XMLParseError::ErrorType type, const std::string& msg) -> void {
            lastError = XMLParseError(type, position, msg);
        }

        auto SkipWhitespace() -> void {
            while (position < content.length() && std::isspace(content[position])) {
                ++position;
            }
        }

        auto ReadNodeName() -> std::string {
            std::string name = "";
            while (position < content.length() && !std::isspace(content[position]) && 
            content[position] != '>' && content[position] != '/' ) {
                name += content[position++];
            }
            return IsValidTag(name) ? name : std::string{};
        }

        auto ReadUntil(char delimiter) -> std::string {
            std::string result = "";
            while (position < content.length() && content[position] != delimiter) {
                result += content[position++];
            }
            return result;
        }

        auto IsEndOfTag() -> bool {
            SkipWhitespace();
            return position < content.length() && content[position] == '>';
        }

        auto ParseAttributes(XMLNode& node) -> bool {
            while (position < content.length() 
            && content[position] != '>' && content[position] != '/') {
                SkipWhitespace();
                std::string name = ReadUntil('=');
                if (name.empty()) return false;

                if (position >= content.length() || content[position] != '=') return false;
                ++position;

                if (position >= content.length() || content[position] != '"') return false;
                ++position;

                std::string value = ReadUntil('"');
                if (position >= content.length()) return false;
                ++position;

                node.AddAttribute(std::move(name),std::move(value));
            }
            return true;
        }

        auto IsValidTag(std::string_view tag) const -> bool {
            if(tag.empty()) return false;

            if(!std::isalpha(tag[0]) && tag[0] != '_') return false;

            return std::all_of(tag.begin() + 1, tag.end(), [](char c) {
                return std::isalnum(c) || c == '-' || c == '.';
            });
        }

        auto SkipComment() -> bool {
            if (position +3 >= content.length() || content.substr(position,4) != "<!--") {
                SetError(XMLParseError::ErrorType::InvalidComment, "Invalid comment start");
                return false;
            }

            position += 4;

            while (position + 2 < content.length()) {
                if (content.substr(position,3) == "-->") {
                    position +=3;
                    return true;
                }
                ++position;
            }

            SetError(XMLParseError::ErrorType::UnclosedTag, "Unclosed comment");
            return false;
        }

        auto ParseCDATA() -> std::string {
            if (position + 8 >= content.length() || content.substr(position,9) != "<![CDATA[" ) {
                SetError(XMLParseError::ErrorType::InvalidCDATA, "Invalid CDATA section start");
                return {};
            }

            position += 9;
            std::string cDataContent = "";

            while (position + 2 < content.length()) {
                if (content.substr(position,3) == "]]>") {
                    position += 3;
                    return cDataContent;
                }
                cDataContent += content[position++];
            }

            SetError(XMLParseError::ErrorType::UnclosedTag, "Unclosed CDATA section");
            return {};
        }

        auto ParseNodeContent(XMLNode &node) -> std::string {
            std::string nodeContent = "";

            while (position < content.length()) {
                if (content[position] == '<') {

                    if (position + 8 < content.length() && content.substr(position,9) == "<![CDATA[") {
                        auto cdata = ParseCDATA();
                        if (lastError) return {};
                        nodeContent += cdata;
                        continue;
                    }

                    if (position + 3 < content.length() && content.substr(position,4) == "<!--") {
                        if (!SkipComment()) {
                            return {};
                        }

                        continue;
                    }

                    if (position + 1 < content.length() && content[position + 1] == '/') {
                        position += 2;
                        auto closingTag = ReadUntil('>');
                        if (closingTag != node.GetName()) {
                            SetError(XMLParseError::ErrorType::MalformedXML, 
                            "Mismatched closing tag: expected " + node.GetName());
                            return {};
                        }
                        ++position;
                        return nodeContent;
                    }

                    auto childNode = ParseNode();
                    if (!childNode) {
                        return {};
                    }
                    node.AddChild(std::move(childNode));
                } else {
                    nodeContent += content[position++];
                }
            }

            SetError(XMLParseError::ErrorType::UnclosedTag, "No closing tag found for " + node.GetName());
            return nodeContent;
        }

        auto ParseNode() -> std::unique_ptr<XMLNode> {
            if (position >= content.length()) {
                SetError(XMLParseError::ErrorType::UnexpectedEOF, "Unexpected end of file while parsing node");
                return nullptr;
            }

            ++position;
            std::string nodeName = ReadNodeName();

            if (nodeName.empty()) {
                SetError(XMLParseError::ErrorType::InvalidTag, "Invalid tag name");
                return nullptr;
            }

            auto node = std::make_unique<XMLNode>(std::move(nodeName));

            SkipWhitespace();

            if (!ParseAttributes(*node)) {
                SetError(XMLParseError::ErrorType::InvalidAttribute, "Error parsing attributes");
                return nullptr;
            }

            if (!IsEndOfTag()) {
                SetError(XMLParseError::ErrorType::MalformedXML, "Expected '>' at end of tag");
                return nullptr;
            }

            ++position;

            std::string nodeContent = ParseNodeContent(*node);
            node -> SetContent(std::move(nodeContent));

            return node;
        }

    public:
        explicit XMLParser(std::string_view xmlContent) : content(xmlContent) {}

        auto GetLastError() const -> const std::optional<XMLParseError>& {
            return lastError;
        }

        auto Parse() -> std::unique_ptr<XMLNode> {
            SkipWhitespace();
            if (position >= content.length() || content[position] != '<') {
                return nullptr;
            }
            return ParseNode();
        }

        auto ValidateXML() -> bool {
            position = 0;
            lastError = std::nullopt;
            SkipWhitespace();

            if(position + 4 < content.length() && content.substr(position,5) == "<?xml") {
                while (position < content.length() && content[position] != '>') {
                    ++position;
                }
                if (position >= content.length()) {
                    SetError(XMLParseError::ErrorType::UnclosedTag, "Unclosed XML declaration");
                    return false;
                }
                ++position;
                SkipWhitespace();
            }

            auto root = Parse();
            return root != nullptr && !lastError;
        }

        static auto IsXMLFile(std::string_view filename) -> bool {
            auto positionOfExtension = filename.rfind('.');
            if (positionOfExtension == std::string_view::npos) return false;

            auto extension = filename.substr(positionOfExtension + 1);
            return extension == "xml";
        }
};

class XMLBuilder {
    private:
        std::unique_ptr<XMLNode> root;
        XMLNode* current;
        std::vector<XMLNode*> nodeStack; 

    public:
        XMLBuilder(std::string rootName) 
        : root(std::make_unique<XMLNode>(std::move(rootName))), current(root.get()) {
            nodeStack.push_back(current);
        }

        auto AddChild(std::string name) -> XMLBuilder& {
            auto child = std::make_unique<XMLNode>(std::move(name));
            auto childPointer = child.get();
            current -> AddChild(std::move(child));
            nodeStack.push_back(current);
            current = childPointer;
            return *this;
        }

        auto AddAttribute(std::string key, std::string value) -> XMLBuilder& {
            current->AddAttribute(std::move(key), std::move(value));
            return *this;
        }

        auto SetContent(std::string content) -> XMLBuilder& {
            current->SetContent(std::move(content));
            return *this;
        }

        auto Up() -> XMLBuilder& {
            if (!nodeStack.empty()) {
                current = nodeStack.back();
                nodeStack.pop_back();
            }
            return *this;
        }

        auto ToRoot() -> XMLBuilder& {
            while (!nodeStack.empty()) {
                Up();
            }
            return *this;
        }

        auto GetCurrentNode() const -> const XMLNode* {
            return current;
        }

        auto Build() -> std::unique_ptr<XMLNode> {
            return std::move(root);
        }
};