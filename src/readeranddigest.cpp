#pragma once
#include "crc.hpp"
#include <istream>

template <typename Reader>

class CRCDigestReader {
    private:
    Reader& reader;
    CRC& digest;

    public:
    CRCDigestReader(Reader &underlyingReader, CRC &crcDigest) : 
    reader(underlyingReader), digest(crcDigest)
    {}

    template <typename Buffer>
    std::size_t read(Buffer &buffer, std::size_t size) {
        auto bytesRead = reader.read(reinterpret_cast<char*>(buffer.data()), size);

        if (bytesRead > 0) {
            digest.update(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer.data()), bytesRead));
        }

        return bytesRead;
    }

    std::size_t read(uint8_t *buffer, std::size_t size) {
        std::size_t bytesRead = reader.read(reinterpret_cast<char*>(buffer),size);

        if (bytesRead > 0) {
            digest.update(std::span<const uint8_t>(buffer,bytesRead));
        }

        return bytesRead;
    }

    uint32_t getCRC() const {
        return digest.finalize();
    }
};

template <typename Reader>

CRCDigestReader<Reader> makeCRCDigestReader(Reader &reader, CRC& digest) {
    return CRCDigestReader<Reader>(reader,digest);
}