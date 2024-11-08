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
        std::vector<EPUBTOCItem> tableOfContents;

        
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
            std::cout << "\nTotal files found: " << fileHeaders.size() << std::endl;
        }

        [[nodiscard]] bool extractUncompressedFile(const LocalFileHeader& header, const std::filesystem::path &outputPath) {
            auto fullPath = outputPath / header.fileName;
            std::filesystem::create_directories(fullPath.parent_path());
            if (header.compressionMethod != 0) {
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
                        }
                    } catch (const std::exception& exception) {
                        std::cerr << exception.what() << '\n';
                    }
                }
            }

            epubFile.seekg(currentPosition);
            return true;
        }      
};