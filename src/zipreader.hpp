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
    };

    std::vector<LocalFileHeader> fileHeaders;

    template <typename RawType>
    requires std::is_trivially_copyable_v<RawType>
    [[nodiscard]] bool readRaw(RawType &value) {
        return static_cast<bool>(epubFile.read(reinterpret_cast<char*>(&value),sizeof(value)));
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
};