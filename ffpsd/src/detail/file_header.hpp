#ifndef FFPSD_DETAIL_FILE_HEADER_HPP_
#define FFPSD_DETAIL_FILE_HEADER_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    constexpr std::uint16_t kVersionPsd = 1;
    constexpr std::uint16_t kVersionPsb = 2;

    // Section 1, a fixed 26 bytes at the start of every file.
    struct FileHeader
    {
        std::uint16_t version = kVersionPsd;
        // The canvas and the composite's channels, alpha ones included.
        ImageInfo info;
    };
    // Throws std::runtime_error for whatever no PSD or PSB holds.
    FileHeader ParseFileHeader(BigEndianReader& reader);
    void WriteFileHeader(BigEndianWriter& writer, const FileHeader& header);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FILE_HEADER_HPP_
