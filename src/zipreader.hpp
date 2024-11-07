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

            const XMLNode* metaDataNode = nullptr;
            for (const auto& child : opfNode.GetChildren()) {
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

            auto getDCElement = [&](const std::string& elementName) -> std::string {
                for (const auto& child : metaDataNode->GetChildren()) {
                    if (child->GetName() == "dc" + elementName || child->GetName() == elementName ||
                    child->GetName() == "opf:" + elementName || child->GetName() == "dc:" + elementName) {
                        return child->GetContent();
                    }
                }

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

        [[nodiscard]] bool processEPUBMetaData() {
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

            std::cout << "Parsed XML structure:\n" << containerXML->ToString() << '\n';
            

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


            return true;
        }
};