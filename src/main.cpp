#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.epub>" << std::endl;
        return 1;
    }

    std::string fileName = argv[1];
    std::string extension = fileName.substr(fileName.find_last_of(".")+1);
    if (extension != "epub") {
        std::cerr << "Error: File must have .epub extension" << std::endl;
        return 1;
    }

    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << fileName << std::endl;
        return 1;
    }

    std::cout << "Successfully opened EPUB file: " << fileName << std::endl;

    file.close();

    return 0;
}