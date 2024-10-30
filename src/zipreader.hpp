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
        epubFile.read(reinterpret_cast<char*>(&signature),sizeof(signature));
        return signature == 0x04034b50;
    }

    void close() {
        if (epubFile.is_open()) {
            epubFile.close();
        }
    }
};