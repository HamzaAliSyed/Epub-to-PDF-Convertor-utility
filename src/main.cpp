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

            std::cout << "\nProcessing EPUB metadata...\n";

            if (!reader.processEPUBMetaData()) {
                std::cerr << "Failed to process EPUB metadata\n";
                return 1;
            }

            std::cout << "\nParsing table of contents...\n";
            if (reader.ParseTOC()) {
                std::cout << "Successfully parsed table of contents\n";
            } else {
                std::cout << "No table of contents found or failed to parse\n";
            }

            std::cout << "\nProcessing content files...\n";

            if (reader.ProcessContentFiles()) {
                std::cout << "Successfully processed content files\n";
            } else {
                std::cout << "Some content files could not be processed\n";
            }

            std::cout << "\nProcessing stylesheets...\n";

            if (reader.ProcessStylesheets()) {
                std::cout << "Successfully processed stylesheets\n";
            } else {
                std::cout << "No stylesheets found or failed to process\n";
            }


            reader.PrintContentSummary();
        }

    } catch (std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}