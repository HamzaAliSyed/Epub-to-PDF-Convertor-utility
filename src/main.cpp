#include <iostream>
#include "zipreader.hpp"
#include "xmlparser.hpp"
#include "xmlvalidator.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.epub>" << std::endl;
        return 1;
    }

    try {
        ZipReader reader(argv[1]);

        if (!reader.isZipFile()) {
            throw std::runtime_error("Not a valid ZIP/EPUB file!");
        }

        std::cout << "This is a valid ZIP/EPUB file!\n";

         if (reader.readAllHeaders()) {
            reader.printAllFiles();

            if (!reader.processEPUBMetaData()) {
                std::cerr << "Failed to process EPUB metadata\n";
                return 1;
            }

            std::cout << "\nExtracting uncompressed files...\n";
            if (!reader.extractAllUncompressedFiles("output")) {
                std::cerr << "Failed to extract some files\n";
                return 1;
            }
        }

    } catch (std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}