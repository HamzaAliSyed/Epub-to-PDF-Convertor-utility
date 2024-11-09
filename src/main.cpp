#include <iostream>
#include <iomanip>
#include "xmlparser.hpp"

auto printCodePoint(uint32_t codePoint) -> void {
    std::cout << "Codepoint: 0x" << std::setfill('0') << std::setw(4) << std::hex << codePoint;

    if (codePoint >= 32 && codePoint <= 126) {
        std::cout << " (" << static_cast<char>(codePoint) << ")";
    } else {
        char utf8[5] = {0};  
        if (codePoint <= 0x7F) {
            utf8[0] = static_cast<char>(codePoint);
        } else if (codePoint <= 0x7FF) {
            utf8[0] = static_cast<char>(0xC0 | (codePoint >> 6));
            utf8[1] = static_cast<char>(0x80 | (codePoint & 0x3F));
        } else if (codePoint <= 0xFFFF) {
            utf8[0] = static_cast<char>(0xE0 | (codePoint >> 12));
            utf8[1] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            utf8[2] = static_cast<char>(0x80 | (codePoint & 0x3F));
        } else if (codePoint <= 0x10FFFF) {
            utf8[0] = static_cast<char>(0xF0 | (codePoint >> 18));
            utf8[1] = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            utf8[2] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            utf8[3] = static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        std::cout << " (" << utf8 << ")";
    }

    std::cout << std::dec << std::endl;
}

auto printString(const std::string& inputString) -> void {
    std::cout << "\nOriginal string: ";

    for (unsigned char letter : inputString) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(letter) << " ";
    }
    std::cout << std::dec << std::endl;

    std::string_view stringView = inputString;
    UTF8Iterator it(stringView.begin(), stringView.end());
    UTF8Iterator end(stringView.end(), stringView.end());

    try {
        std::cout << "Decoded characters:\n";
        while (it != end) {
            printCodePoint(it.CurrentCharacter());
            ++it;
        }
    } catch (const XMLParseError& exception) {
        std::cerr << "Error: " << exception.what() << std::endl;
    }
    
    std::cout << "-------------------" << std::endl;
}

int main() {
    #ifdef _WIN32
    system("chcp 65001");
    #endif

    std::vector<std::string> tests = {
        std::string("Hello, World!"),
        std::string("Hello 世界! 🌍"),
        std::string("Testing → ♠ ♣ ♥ ♦"),
        std::string("Mixed ASCII and UTF8 → お疲れ様です"),
        std::string("Emojis: 👨‍👩‍👧‍👦 🌈 🎉")
    };

    for (const auto& test : tests) {
        printString(test);
    }

    auto document = std::make_shared<XMLNode> (XMLNodeType::Document);
    auto root = std::make_shared<XMLNode>(XMLNodeType::Element);

    root->SetName("root");

    auto child = std::make_shared<XMLNode>(XMLNodeType::Element);
    child->SetName("child");

    auto text = std::make_shared<XMLNode>(XMLNodeType::Text);
    text->SetName("Hello, UTF-8 World! 世界");

    document->AddChild(root);
    root->AddChild(child);
    child->AddChild(text);

    std::cout << "Root name: " << document->GetChildren()[0]->GetName() << "\n";
    std::cout << "Child name: " << document->GetChildren()[0]->GetChildren()[0]->GetName() << "\n";
    std::cout << "Text content: " << document->GetChildren()[0]->GetChildren()[0]->GetChildren()[0]->GetName() << "\n";

    std::string simpleXML = "<root>Hello</root>";
    XMLParser parser(simpleXML);

    try {
        auto doc = parser.Parse();
        std::cout << "Root element name: " << doc->GetChildren()[0]->GetName() << std::endl;
    } catch (const XMLParseError& exception) {
        std::cout << "Parse error: " << exception.what() << std::endl;
    }

    return 0;
}