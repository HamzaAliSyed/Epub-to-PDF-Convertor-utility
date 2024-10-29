#pragma once
#include <cstddef>
#include <span>
#include <vector>

template <typename Reader>

class CountBufferReader {
    private:

    Reader &reader;
    std::size_t count;
    

    public:

    explicit CountBufferReader(Reader& underlyingReader) : reader(underlyingReader), count(0) {

    }

    [[nodiscard]] std::size_t getCount() const {
        return count;
    }

    [[nodiscard]] std::span<const uint8_t> fillBuffer() {
        return reader.fillBuffer();
    }

    void consume(std::size_t amount) {
        reader.consume(amount);
        count += amount;
    }

    template<typename Buffer>
    std::size_t read(Buffer& buffer, std::size_t size) {
        auto bytesRead = reader.read(buffer, size);
        count += bytesRead;
        return bytesRead;
    }

    std::size_t readUntilFull(std::span<uint8_t> outputBuffer) {
        std::size_t totalBytesRead = 0;

        while(totalBytesRead < outputBuffer.size()) {
            auto buffer = fillBuffer();
            if (buffer.empty()) {
                break;
            }

            std::size_t bytesToCopy = std::min(buffer.size(),outputBuffer.size() - totalBytesRead);

            std::copy_n (buffer.begin(), bytesToCopy, outputBuffer.begin() + totalBytesRead);

            consume(bytesToCopy);
            totalBytesRead += bytesToCopy;
        }

        return totalBytesRead;
    }
};

template<typename Reader>
CountBufferReader<Reader> makeCountBufferReader(Reader& reader) {
    return CountBufferReader<Reader>(reader);
}