#include <iostream>
#include "zipreader.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.epub>" << std::endl;
        return 1;
    }

    ZipReader reader(argv[1]);
    if (reader.isZipFile()) {
        std::cout << "This is a valid ZIP/EPUB file!" << std::endl;
        if (reader.readFileHeaders()) {
            reader.printAllFiles();
        }
    } else {
        std::cout << "This is NOT a valid ZIP/EPUB file!" << std::endl;
    }

    reader.close();
    return 0;
}