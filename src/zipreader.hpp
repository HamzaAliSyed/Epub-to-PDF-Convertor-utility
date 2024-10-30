#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iostream>

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

    public:
    ZipReader(const std::string &fileName) {
        epubFile.open(fileName, std::ios::binary);
        if (!epubFile.is_open()) {
            std::cerr << "Cannot open file: " << fileName << std::endl;
            return;
        }
    }

    ~ZipReader() {
        if (epubFile.is_open()) {
            epubFile.close();
        }
    }

    bool isZipFile() {
        uint32_t signature;
        epubFile.seekg(0); 
        epubFile.read(reinterpret_cast<char*>(&signature),sizeof(signature));
        return signature == 0x04034b50;
    }

    void close() {
        if (epubFile.is_open()) {
            epubFile.close();
        }
    }

    bool readFileHeaders() {
        epubFile.seekg(0);
        
        while(true) {
            LocalFileHeader header;

            epubFile.read(reinterpret_cast<char*>(&header.signature), sizeof(header.signature));
            if (epubFile.eof() || header.signature != 0x04034b50) {
                break;
            }

            epubFile.read(reinterpret_cast<char*>(&header.versionNeeded), sizeof(header.versionNeeded));
            epubFile.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));
            epubFile.read(reinterpret_cast<char*>(&header.compressionMethod), sizeof(header.compressionMethod));
            epubFile.read(reinterpret_cast<char*>(&header.lastModifiedTime), sizeof(header.lastModifiedTime));
            epubFile.read(reinterpret_cast<char*>(&header.lastModifiedDate), sizeof(header.lastModifiedDate));
            epubFile.read(reinterpret_cast<char*>(&header.crc32), sizeof(header.crc32));
            epubFile.read(reinterpret_cast<char*>(&header.compressedSize), sizeof(header.compressedSize));
            epubFile.read(reinterpret_cast<char*>(&header.uncompressedSize), sizeof(header.uncompressedSize));
            epubFile.read(reinterpret_cast<char*>(&header.fileNameLength), sizeof(header.fileNameLength));
            epubFile.read(reinterpret_cast<char*>(&header.extraFieldLength), sizeof(header.extraFieldLength));

            if (header.fileNameLength > 0) {
                std::vector<char> filename(header.fileNameLength);
                epubFile.read(filename.data(), header.fileNameLength);
                header.fileName = std::string(filename.data(), header.fileNameLength);
            }

            if (header.extraFieldLength > 0) {
                epubFile.seekg(header.extraFieldLength, std::ios::cur);
            }

            epubFile.seekg(header.compressedSize, std::ios::cur);

            fileHeaders.push_back(header);
        }

        return !fileHeaders.empty();
    }

        void printAllFiles() {
        for (const auto& header : fileHeaders) {
            std::cout << "\nFile Information:" << std::endl;
            std::cout << "Compression method: " << header.compressionMethod << std::endl;
            std::cout << "Compressed size: " << header.compressedSize << " bytes" << std::endl;
            std::cout << "Uncompressed size: " << header.uncompressedSize << " bytes" << std::endl;
            std::cout << "Filename: " << header.fileName << std::endl;
        }
        std::cout << "\nTotal files found: " << fileHeaders.size() << std::endl;
    }
};