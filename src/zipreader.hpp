#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <span>
#include <concepts>
#include <format>
#include <algorithm>
#include <set>
#include "xmlparser.hpp"
#include "xmlvalidator.hpp"


class ZipReader{
    private:
        std::ifstream epubFile;

        struct LocalFileHeader {
            uint32_t signature;
            uint16_t versionNeeded;
            uint16_t flags;
            uint16_t compressionMethod;
            uint16_t lastModifiedTime;
            uint16_t lastModifiedDate;
            uint32_t crc32;
            uint32_t compressedSize;
            uint32_t uncompressedSize;
            uint16_t fileNameLength;
            uint16_t extraFieldLength;
            std::string fileName;
            std::streampos dataStart;
        };

        std::vector<LocalFileHeader> fileHeaders;

        template <typename RawType>
        requires std::is_trivially_copyable_v<RawType>
        [[nodiscard]] bool readRaw(RawType &value) {
            return static_cast<bool>(epubFile.read(reinterpret_cast<char*>(&value),sizeof(value)));
        }

        [[nodiscard]] std::string getOPFPath(const XMLNode& containerXML) {
            std::cout << "Looking for OPF path in XML:\n" << containerXML.ToString() << "\n";

            for(const auto& child : containerXML.GetChildren()) {
                if(child->GetName().find("rootfiles") != std::string::npos) {
                    for(const auto& rootfile : child->GetChildren()) {
                        if(rootfile->GetName().find("rootfile") != std::string::npos) {
                            auto fullPath = rootfile->GetAttributeValue("full-path");
                            if (!fullPath.empty()) {
                                return std::string(fullPath);
                            }
                        }
                    }
                }
            }

            return {};
        }

        struct EPUBMetaData {
            std::string title;
            std::string creator;
            std::string language;
            std::string identifier;
            std::string publisher;
            std::string dateOfPublication;
        };

        struct EPUBFont {
            std::string href;
            std::string family;
            std::string style; 
            size_t size{0}; 
        };

        std::map<std::string, EPUBFont> fonts;

        [[nodiscard]] auto parseOPFFile(const std::string& opfPath) -> std::unique_ptr<XMLNode> {
            auto opfIt = std::find_if(
                fileHeaders.begin(), fileHeaders.end(),
                [&opfPath](const LocalFileHeader& header) {
                    return header.fileName == opfPath;
                }
            );

            if (opfIt == fileHeaders.end()) {
                std::cerr << "OPF file not found: " << opfPath << '\n';
                return nullptr;
            }

            return extractAndParseXML(*opfIt);
        }

        [[nodiscard]] auto ExtractMetaData(const XMLNode& opfNode) -> EPUBMetaData {
            EPUBMetaData metaData;

            std::cout << "\nSearching for metadata node...\n";
            const XMLNode* metaDataNode = nullptr;
            for (const auto& child : opfNode.GetChildren()) {
                std::cout << "Found node: '" << child->GetName() << "'\n";
                if (child->GetName() == "metadata" || child->GetName().find(":metadata") != std::string::npos 
                || child->GetName() == "opf:metadata") {
                    metaDataNode = child.get();
                    std::cout << "Found metadata node: " << child->GetName() << "\n";
                    break;
                }
            }

            if (!metaDataNode) {
                std::cerr << "No metadata section found in OPF\n";
                return metaData;
            }

            std::cout << "\nMetadata children:\n";
            for (const auto& child : metaDataNode->GetChildren()) {
                std::cout << "Child name: '" << child->GetName() << "' Content: '" << child->GetContent() << "'\n";
            }

            auto getDCElement = [&](const std::string& elementName) -> std::string {
                std::cout << "\nLooking for element: " << elementName << "\n";
                for (const auto& child : metaDataNode->GetChildren()) {
                    std::cout << "Checking node: '" << child->GetName() << "'\n";

                    if (child->GetName() == "dc:" + elementName) {
                        std::cout << "Found exact match dc:" << elementName << "\n";
                        return child->GetContent();
                    }

                    if (child->GetName() == elementName) {
                        std::cout << "Found exact match " << elementName << "\n";
                        return child->GetContent();
                    }

                    if (child->GetName() == "dc" + elementName) {
                        std::cout << "Found match dc" << elementName << "\n";
                        return child->GetContent();
                    }

                    if (child->GetName() == "opf:" + elementName) {
                        std::cout << "Found match opf:" << elementName << "\n";
                        return child->GetContent();
                    }
                }

                std::cout << "No match found for " << elementName << "\n";

                return {};
            };

            metaData.title = getDCElement("title");
            metaData.creator = getDCElement("creator");
            metaData.language = getDCElement("language");
            metaData.identifier = getDCElement("identifier");
            metaData.publisher = getDCElement("publisher");
            metaData.dateOfPublication = getDCElement("date");

            return metaData;
        }

        struct EPUBManifestItem {
            std::string id;
            std::string href;
            std::string mediaType;
            std::optional<size_t> size;
            bool processed{false};
        };

        struct EPUBSpineItem {
            std::string idref;
            bool linear;
            std::optional<size_t> index;
        };

        struct EPUBTOCItem {
            std::string title;
            std::string href;
            size_t level{0};
            std::vector<EPUBTOCItem> children;
        };

        struct EPUBStyleSheet {
            std::string href;
            std::string content;
            std::vector<std::string> affectedFiles;
        };

        struct EPUBContent {
            std::string title;
            std::string textContent;
            std::vector<std::string> images;
            std::vector<std::string> links;
            std::optional<size_t> spineIndex;
            std::vector<std::string> styleSheets;
            std::string sanitizedContent;
            std::optional<size_t> wordCount;
            std::optional<size_t> characterCount;
        };



        std::vector<EPUBManifestItem> manifestItems;
        std::vector<EPUBSpineItem> spineItems;
        std::map<std::string, EPUBContent> contentMap;
        std::vector<EPUBTOCItem> tableOfContents;
        std::map<std::string, EPUBStyleSheet> stylesheets;

        auto ParseManifest(const XMLNode& opfNode) -> bool {
            const XMLNode* manifestNode = nullptr;

            for (const auto& child : opfNode.GetChildren()) {
                if (child->GetName() == "manifest" || child->GetName().find(":manifest") != std::string::npos) {
                    manifestNode = child.get();
                    break;
                }
            }

            if (!manifestNode) {
                std::cerr << "No manifest section found in OPF\n";
                return false;
            }

            for (const auto& item : manifestNode->GetChildren()) {
                if (item->GetName() == "item") {
                    EPUBManifestItem manifestItem;
                    manifestItem.id = std::string(item->GetAttributeValue("id"));
                    manifestItem.href = std::string(item->GetAttributeValue("href"));
                    manifestItem.mediaType = std::string(item->GetAttributeValue("media-type"));

                    auto fileIt = std::find_if(
                        fileHeaders.begin(),
                        fileHeaders.end(),
                        [&](const LocalFileHeader& header) {
                            return header.fileName.ends_with(manifestItem.href);
                        }
                    );

                    if (fileIt != fileHeaders.end()) {
                        manifestItem.size = fileIt->uncompressedSize;
                    }

                    manifestItems.push_back(std::move(manifestItem));
                    std::cout << "Added manifest item: " << manifestItems.back().href 
                    << " (" << manifestItems.back().mediaType << ")\n";
                }
            }

            return !manifestItems.empty();
        }

        auto ParseSpine(const XMLNode& opfNode) -> bool {
            const XMLNode* spineNode = nullptr;
            std::cout << "\nParsing spine section...\n";

            for (const auto& child : opfNode.GetChildren()) {
                if (child->GetName() == "spine" || child->GetName().find(":spine") != std::string::npos) {
                    spineNode = child.get();
                    break;
                }
            }

            if (!spineNode) {
                std::cerr << "No spine section found in OPF\n";
                return false;
            }

            for (const auto& itemref : spineNode->GetChildren()) {
                if (itemref->GetName() == "itemref") {
                    EPUBSpineItem spineItem;
                    spineItem.idref = std::string(itemref->GetAttributeValue("idref"));
                    auto linear = itemref->GetAttributeValue("linear");
                    spineItem.linear = linear.empty() || linear == "yes";
                    spineItems.push_back(std::move(spineItem));
                }
            }

            return true;
        }

        auto ExtractTextContent(const XMLNode& node) -> std::string {
            std::string content;

            if (!node.GetContent().empty()) {
                content += node.GetContent() + " ";
            }

            for (const auto& child : node.GetChildren()) {
                content += ExtractTextContent(*child);
            }

            return content;
        }

        auto ExtractImages(const XMLNode& node) -> std::vector<std::string> {
            std::vector<std::string> images;

            if (node.GetName() == "img") {
                auto source = node.GetAttributeValue("src");
                if (!source.empty()) {
                    images.push_back(std::string(source));
                }
            }

            for (const auto& child : node.GetChildren()) {
                auto childImages = ExtractImages(*child);
                images.insert(images.end(), childImages.begin(), childImages.end());
            }

            return images;
        }

        auto ExtractLinks(const XMLNode& node) -> std::vector<std::string> {
            std::vector<std::string> links;
            if (node.GetName() == "a") {
                auto href = node.GetAttributeValue("href");
                if (!href.empty()) {
                    links.push_back(std::string(href));
                }
            }

            for (const auto& child : node.GetChildren()) {
                auto childLinks = ExtractLinks(*child);
                links.insert(links.end(), childLinks.begin(), childLinks.end());
            }

            return links;
        }

        auto ProcessHTMLContent(const EPUBManifestItem& manifestItem,const LocalFileHeader& contentFile,
        EPUBContent& content) -> bool {
            auto contentXML = extractAndParseXML(contentFile);
            if (!contentXML) {
                std::cerr << "Failed to parse content file: " << manifestItem.href << '\n';
                return false;
            }

            if (auto titleNode = contentXML->FindChildByName("title")) {
                content.title = titleNode->GetContent();
            }

            content.textContent = ExtractTextContent(*contentXML);
            content.images = ExtractImages(*contentXML);
            content.links = ExtractLinks(*contentXML);

            for (const auto& child : contentXML->GetChildren()) {
                if (
                    child->GetName() == "link" &&
                    child->GetAttributeValue("rel") == "stylesheet"
                ) {
                    auto href = child->GetAttributeValue("href");
                    if (!href.empty()) {
                        content.styleSheets.push_back(std::string(href));
                    }
                }
            }

            content.sanitizedContent = SanitizeContent(*contentXML);
            content.characterCount = content.textContent.length();
            content.wordCount = std::count_if(
                content.textContent.begin(),
                content.textContent.end(),
                [](char c) { return std::isspace(c); }
            ) + 1;

            return true;
        }

        auto ProcessFontFile(const EPUBManifestItem& manifestItem,
        const LocalFileHeader& contentFile) -> void {
            EPUBFont font;
            font.href = manifestItem.href;
            font.size = contentFile.uncompressedSize;
            std::filesystem::path fontPath(manifestItem.href);
            std::string filename = fontPath.stem().string();

            auto dashPosition = filename.find('-');
            if (dashPosition != std::string::npos) {
                font.family = filename.substr(0, dashPosition);
                font.style = filename.substr(dashPosition + 1);
            } else {
                font.family = filename;
                font.style = "Regular";
            }

            fonts[manifestItem.href] = std::move(font);
            std::cout << "Processed font: " << font.family << " (" << font.style << ")\n";
        }

        auto SanitizeContent(const XMLNode& node) -> std::string {
            static const std::set<std::string> allowedTags = {
                "p", "div", "span", "h1", "h2", "h3", "h4", "h5", "h6",
                "ul", "ol", "li", "a", "img", "em", "strong", "br"
            };

            std::string result;

            if (allowedTags.contains(node.GetName())) {
                result += "<" + node.GetName();

                for (const auto& [key, value] : node.GetAttributes()) {
                    if (node.GetName() == "a" && key == "href") {
                        result += " href=\"" + value + "\"";
                    } else if (node.GetName() == "img" && (key == "src" || key == "alt")) {
                        result += " " + key + "=\"" + value + "\"";
                    }
                }

                result += ">";
            }

            if (!node.GetContent().empty()) {
                std::string content = node.GetContent();
                content.erase(
                    std::remove_if(
                        content.begin(), content.end(),
                        [](char c) { return !std::isprint(c) && !std::isspace(c); }
                        ), content.end()
                    );
                result += content;
            }

            for (const auto& child : node.GetChildren()) {
                result += SanitizeContent(*child);
            }

            if (allowedTags.contains(node.GetName())) {
                result += "</" + node.GetName() + ">";
            }

            return result;
        }


    public:
        explicit ZipReader(const std::filesystem::path &filePath) {
            epubFile.open(filePath, std::ios::binary);
            if (!epubFile.is_open()) {
                throw std::runtime_error(std::format("Cannot open file: {}", filePath.string()));
            }
        }

        ZipReader(const ZipReader&) = delete;
        ZipReader& operator=(const ZipReader&) = delete;
        ZipReader(ZipReader&&) noexcept = default;
        ZipReader& operator=(ZipReader&&) noexcept = default;
        ~ZipReader() = default;

        [[nodiscard]] bool isZipFile() {
            uint32_t signature;
            epubFile.seekg(0); 
            return readRaw(signature) && signature == 0x04034b50;
        }

        void close() {
            if (epubFile.is_open()) {
                epubFile.close();
            }
        }

        [[nodiscard]] bool readAllHeaders() {
            epubFile.seekg(0);
            
            while(true) {
                LocalFileHeader header{};
                auto headerStart = epubFile.tellg();

                if (!readRaw(header.signature) || header.signature != 0x04034b50) {
                    break;
                }

                if (
                    !readRaw(header.versionNeeded) ||
                    !readRaw(header.flags) ||
                    !readRaw(header.compressionMethod) ||
                    !readRaw(header.lastModifiedDate) ||
                    !readRaw(header.lastModifiedTime) ||
                    !readRaw(header.crc32) ||
                    !readRaw(header.compressedSize) ||
                    !readRaw(header.uncompressedSize) ||
                    !readRaw(header.fileNameLength) || 
                    !readRaw(header.extraFieldLength)
                ) {
                    break;
                }

                if (header.fileNameLength > 0) {
                    std::vector<char> filename(header.fileNameLength);
                    if (!epubFile.read(filename.data(), header.fileNameLength)) {
                        break;
                    }
                    header.fileName = std::string(filename.data(), header.fileNameLength);
                }

                if (header.extraFieldLength > 0) {
                    epubFile.seekg(header.extraFieldLength, std::ios::cur);
                }

                header.dataStart = headerStart + static_cast<std::streamoff>(
                    30 +  header.fileNameLength + header.extraFieldLength
                );

                epubFile.seekg(header.compressedSize, std::ios::cur);

                fileHeaders.push_back(std::move(header));
            }

            return !fileHeaders.empty();
        }

            void printAllFiles() const {
            for (const auto& header : fileHeaders) {
                std::cout << "\nFile Information:" << std::endl;
                std::cout << "Compression method: " << header.compressionMethod << std::endl;
                std::cout << "Compressed size: " << header.compressedSize << " bytes" << std::endl;
                std::cout << "Uncompressed size: " << header.uncompressedSize << " bytes" << std::endl;
                std::cout << "Filename: " << header.fileName << std::endl;
            }
            std::cout << "\nTotal files found: " << fileHeaders.size() << std::endl;
        }

        [[nodiscard]] bool extractUncompressedFile(const LocalFileHeader& header, const std::filesystem::path &outputPath) {
            auto fullPath = outputPath / header.fileName;
            std::filesystem::create_directories(fullPath.parent_path());
            if (header.compressionMethod != 0) {
                std::cout << "Skipping compressed file: " << header.fileName << '\n';
                return false;
            }

            std::ofstream outFile(fullPath, std::ios::binary);
            if (!outFile) {
                throw std::runtime_error(std::format("Cannot create file: {}", fullPath.string()));
            }

            std::vector<std::byte> buffer(header.uncompressedSize);
            if (!epubFile.read(reinterpret_cast<char*>(buffer.data()), header.uncompressedSize)) {
                return false;
            }

            std::span<const std::byte> dataSpan(buffer);
            outFile.write(reinterpret_cast<const char*>(dataSpan.data()), dataSpan.size());

            return true;
        }

        [[nodiscard]] bool extractAllUncompressedFiles(const std::filesystem::path& outputPath) {
            std::filesystem::create_directories(outputPath);

            auto currentPosition = epubFile.tellg();
            epubFile.seekg(0);

            for (const auto &header : fileHeaders) {
                if (header.compressionMethod == 0) {
                    uint32_t dataOffset = sizeof(LocalFileHeader) + header.fileNameLength +  header.extraFieldLength;
                    epubFile.seekg(dataOffset, std::ios::cur);

                    try {
                        if (extractUncompressedFile(header, outputPath)) {
                            std::cout << "Extracted: " << header.fileName << '\n';
                        }
                    } catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                    }
                }
            }

            epubFile.seekg(currentPosition);
            return true;
        }

        [[nodiscard]] std::unique_ptr<XMLNode> extractAndParseXML(const LocalFileHeader& header) {
            if (header.compressionMethod != 0) {
                std::cout << "Cannot parse compressed XML file: " << header.fileName << '\n';
                return nullptr;
            }

            auto currentPosition = epubFile.tellg();
            epubFile.seekg(header.dataStart);

            std::vector<char> buffer(header.uncompressedSize);
            if (!epubFile.read(buffer.data(), header.uncompressedSize)) {
                std::cerr << "Failed to read XML content\n";
                epubFile.seekg(currentPosition);
                return nullptr;
            }

            std::cout << "\nComplete XML content:\n";
            std::string_view fullContent(buffer.data(), header.uncompressedSize);
            std::cout << fullContent << "\n\n";

            epubFile.seekg(currentPosition);

            std::string_view xmlContent(buffer.data(), header.uncompressedSize);
            XMLParser parser(xmlContent);
            auto node = parser.Parse();

            if (!node) {
                if (auto error = parser.GetLastError()) {
                    std::cerr << "Failed to parse XML at position "
                    << error->GetPosition() << ": "
                    << error->GetMessage() << '\n';

                    if (error->GetPosition() < header.uncompressedSize) {
                        std::cout << "Content around error position:\n";
                        size_t start = error->GetPosition() > 10 ? error->GetPosition() - 10 : 0;
                        size_t length = std::min(static_cast<size_t>(20), 
                        static_cast<size_t>(header.uncompressedSize - start));
                        std::cout << std::string_view(buffer.data() + start, length) << '\n';
                    }
                }
            }

            return node;
        }

        [[nodiscard]] auto processEPUBMetaData() -> bool {
            auto containerIt = std::find_if(
                fileHeaders.begin(),fileHeaders.end(),
                [](const LocalFileHeader& header) {
                    return header.fileName == "META-INF/container.xml";
                }
            );

            if (containerIt == fileHeaders.end()) {
                std::cerr << "No container.xml found\n";
                return false;
            }

            std::cout << "Found container.xml, size: " << containerIt->uncompressedSize << " bytes\n";

            auto containerXML = extractAndParseXML(*containerIt);
            if (!containerXML) {
                std::cerr << "Failed to parse container.xml\n";
                return false;
            }
            

            XMLValidator validator;

            XMLValidator::DTDValidator dtd;
            XMLValidator::DTDValidator::ElementRule containerRule;
            containerRule.elementName = "container";
            containerRule.childElements.push_back({"rootfiles",
            XMLValidator::DTDValidator::ElementRule::Occurrence::One});

            XMLValidator::DTDValidator::ElementRule rootfilesRule;
            rootfilesRule.elementName = "rootfiles";
            rootfilesRule.childElements.push_back
            ({"rootfile", XMLValidator::DTDValidator::ElementRule::Occurrence::OneOrMore});

            dtd.AddElementRule(std::move(containerRule));
            dtd.AddElementRule(std::move(rootfilesRule));

            if (!dtd.ValidateNode(*containerXML)) {
                std::cerr << "Invalid container.xml structure\n";
                return false;
            }

            std::string opfPath = getOPFPath(*containerXML);
            if (opfPath.empty()) {
                std::cerr << "Could not find OPF file path in container.xml\n";
                std::cout << "XML Structure for debugging:\n";
                std::cout << containerXML->ToString() << "\n";
                return false;
            }

            std::cout << "Found OPF file: " << opfPath << '\n';

            auto opfNode = parseOPFFile(opfPath);
            if (!opfNode) {
                std::cerr << "Failed to parse OPF file\n";
                return false;
            }

            std::cout << "Parsed OPF structure:\n" << opfNode->ToString() << '\n';

            auto metadata = ExtractMetaData(*opfNode);

            std::cout << "\nEPUB Metadata:\n";
            std::cout << "Title: " << metadata.title << '\n';
            std::cout << "Creator: " << metadata.creator << '\n';
            std::cout << "Language: " << metadata.language << '\n';
            std::cout << "Identifier: " << metadata.identifier << '\n';
            std::cout << "Publisher: " << metadata.publisher << '\n';
            std::cout << "Date: " << metadata.dateOfPublication << '\n';

            if (!ParseManifest(*opfNode)) {
                std::cerr << "Failed to parse manifest\n";
                return false;
            }

            if (!ParseSpine(*opfNode)) {
                std::cerr << "Failed to parse spine\n";
                return false;
            }


            return true;
        }

        auto ProcessStylesheets() -> bool {
            for (const auto& item : manifestItems) {
                if (item.mediaType == "text/css") {
                    auto fileIt = std::find_if(
                        fileHeaders.begin(),
                        fileHeaders.end(),
                        [&](const LocalFileHeader& header) {
                            return header.fileName.ends_with(item.href);
                        }
                    );

                    if (fileIt == fileHeaders.end()) continue;

                    EPUBStyleSheet stylesheet;
                    stylesheet.href = item.href;

                    auto currentPosition = epubFile.tellg();
                    epubFile.seekg(fileIt->dataStart);

                    std::vector<char> buffer(fileIt->uncompressedSize);
                    if (epubFile.read(buffer.data(), fileIt->uncompressedSize)) {
                        stylesheet.content = std::string(buffer.data(), fileIt->uncompressedSize);
                    }

                    epubFile.seekg(currentPosition);
                    stylesheets[item.href] = std::move(stylesheet);
                }
            }

            for (auto& [href, content] : contentMap) {
                for (const auto& stylesheet : content.styleSheets) {
                    if (auto it = stylesheets.find(stylesheet); it != stylesheets.end()) {
                        it->second.affectedFiles.push_back(href);
                    }
                }
            }

            return !stylesheets.empty();
        }

        auto ParseTOC() -> bool {
            auto ncxIt = std::find_if(
                manifestItems.begin(),
                manifestItems.end(),
                [](const EPUBManifestItem& item) {
                    return item.mediaType == "application/x-dtbncx+xml";
                }
            );

            if (ncxIt == manifestItems.end()) {
                std::cerr << "No NCX file found in manifest\n";
                return false;
            }

            auto ncxFile = std::find_if(
                fileHeaders.begin(),
                fileHeaders.end(),
                [&](const LocalFileHeader& header) {
                    return header.fileName.ends_with(ncxIt->href);
                }
            );

            if (ncxFile == fileHeaders.end()) {
                std::cerr << "NCX file not found in archive\n";
                return false;
            }

            auto ncxXML = extractAndParseXML(*ncxFile);
            if (!ncxXML) {
                std::cerr << "Failed to parse NCX file\n";
                return false;
            }

            std::function<void(const XMLNode&, std::vector<EPUBTOCItem>&, size_t)> 
            parseNavPoint = [&](const XMLNode& node, std::vector<EPUBTOCItem>& items, size_t level) {
                if (node.GetName() == "navPoint") {
                    EPUBTOCItem item;
                    item.level = level;

                    if (auto labelNode = node.FindChildByName("navLabel")) {
                        if (auto textNode = labelNode->FindChildByName("text")) {
                            item.title = textNode->GetContent();
                        }
                    }

                    if (auto contentNode = node.FindChildByName("content")) {
                        item.href = std::string(contentNode->GetAttributeValue("src"));
                    }

                    for (const auto& child : node.GetChildren()) {
                        if (child->GetName() == "navPoint") {
                            parseNavPoint(*child, item.children, level + 1);
                        }
                    }
                    items.push_back(std::move(item));
                }
            };

            auto navMapNode = ncxXML->FindChildByName("navMap");
            if (navMapNode) {
                for (const auto& child : navMapNode->GetChildren()) {
                    parseNavPoint(*child, tableOfContents, 0);
                }
            }

            return !tableOfContents.empty();
        }

        auto ProcessContentFiles() -> bool {
            std::cout << "\nProcessing content files...\n";
            bool success = true;

            for (const auto& spineItem : spineItems) {
                auto manifestItem = std::find_if(
                    manifestItems.begin(),
                    manifestItems.end(),
                    [&](const EPUBManifestItem& item) {
                        return item.id == spineItem.idref; 
                    }
                );

                if (manifestItem == manifestItems.end()) {
                    std::cerr << "Spine item not found in manifest: " << spineItem.idref << '\n';
                    continue;
                }

                auto contentFile = std::find_if(
                    fileHeaders.begin(),
                    fileHeaders.end(),
                    [&](const LocalFileHeader& header) {
                        return header.fileName.ends_with(manifestItem->href);
                    }
                );

                if (contentFile == fileHeaders.end()) {
                    std::cerr << "Content file not found: " << manifestItem->href << '\n';
                    continue;
                }

                std::cout << "Processing spine item: " << manifestItem->href << '\n';


                EPUBContent content;
                content.spineIndex = spineItem.index;

                if (manifestItem->mediaType == "application/xhtml+xml" ||
                manifestItem->mediaType == "text/html") {
                    ProcessHTMLContent(*manifestItem, *contentFile, content);
                } 

                contentMap[manifestItem->href] = std::move(content);
                manifestItem->processed = true;
            }

            for (auto& manifestItem : manifestItems) {
                if (manifestItem.processed) continue;

                auto contentFile = std::find_if(
                    fileHeaders.begin(),
                    fileHeaders.end(),
                    [&](const LocalFileHeader& header) {
                        return header.fileName.ends_with(manifestItem.href);
                    }
                );

                if (contentFile == fileHeaders.end()) {
                    std::cerr << "Resource file not found: " << manifestItem.href << '\n';
                    continue;
                }

                std::cout << "Processing resource: " << manifestItem.href << '\n';
                EPUBContent content;

                if (manifestItem.mediaType.starts_with("image/")) {
                    content.images.push_back(manifestItem.href);
                    manifestItem.processed = true;
                } else if (
                    manifestItem.mediaType == "application/x-font-opentype" ||
                    manifestItem.mediaType == "application/x-font-ttf" ||
                    manifestItem.mediaType == "application/vnd.ms-opentype" ||
                    manifestItem.mediaType == "font/ttf" ||
                    manifestItem.mediaType == "font/otf"
                ) {
                    ProcessFontFile(manifestItem, *contentFile);
                    manifestItem.processed = true;
                } else if (manifestItem.mediaType == "application/x-dtbncx+xml") {
                    manifestItem.processed = true;
                } else if (manifestItem.mediaType == "text/css") {
                    manifestItem.processed = true;
                } else if (manifestItem.mediaType == "application/xhtml+xml") {
                    ProcessHTMLContent(manifestItem, *contentFile, content);
                    manifestItem.processed = true;
                }

                if (manifestItem.processed && !content.textContent.empty()) {
                    contentMap[manifestItem.href] = std::move(content);
                }
            }

            for (const auto& item : manifestItems) {
                if (!item.processed) {
                    std::cerr << "Warning: Unknown media type " << item.mediaType
                    << " for file: " << item.href << '\n';
                }
            }

            std::cout << "Processed " << contentMap.size() << " content files\n";
            std::cout << "Found " << fonts.size() << " font files\n";
            return success;
        }


        auto PrintContentSummary() const -> void {
            std::cout << "\nEPUB Content Summary:\n";
            std::cout << "=====================\n";

            std::cout << "Manifest items: " << manifestItems.size() << '\n';
            std::cout << "Spine items: " << spineItems.size() << '\n';
            std::cout << "Content files: " << contentMap.size() << '\n';
            std::cout << "Stylesheets: " << stylesheets.size() << '\n';

            size_t totalWords = 0;
            size_t totalChars = 0;
            size_t totalImages = 0;

            for (const auto& [href, content] : contentMap) {
                if (content.wordCount) totalWords += *content.wordCount;
                if (content.characterCount) totalChars += *content.characterCount;
                totalImages += content.images.size();
            }

            std::cout << "\nTotal statistics:\n";
            std::cout << "- Words: " << totalWords << '\n';
            std::cout << "- Characters: " << totalChars << '\n';
            std::cout << "- Images: " << totalImages << '\n';

            if (!tableOfContents.empty()) {
                std::cout << "\nTable of Contents:\n";
                std::function<void(const EPUBTOCItem&)> printTOC =
                [&](const EPUBTOCItem& item) {
                    std::cout << std::string(item.level * 2, ' ')
                    << "- " << item.title << '\n';
                    for (const auto& child : item.children) {
                        printTOC(child);
                    }
                };

                for (const auto& item : tableOfContents) {
                    printTOC(item);
                }
            }

            if (!fonts.empty()) {
                std::cout << "\nFont files:\n";
                for (const auto& [href, font] : fonts) {
                    std::cout << "- " << font.family << " " << font.style
                    << " (" << (font.size / 1024) << "KB)\n";
                }
            }
        }
};