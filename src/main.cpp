#include <iostream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include "xmlparser.hpp"

auto printCodePoint(uint32_t codePoint) -> void {
    std::cout << "Codepoint: 0x" << std::setfill('0') << std::setw(4) << std::hex << codePoint;

    if (codePoint >= 32 && codePoint <= 126) {
        std::cout << " (" << static_cast<char>(codePoint) << ")";
    } else {
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        try {
            std::string utf8_char = converter.to_bytes(static_cast<char32_t>(codePoint));
            std::cout << " (" << utf8_char << ")";
        } catch (...) {
            
        }
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
    
    return 0;
}