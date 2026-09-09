#ifndef FFPSD_DETAIL_FILE_HEADER_HPP_
#define FFPSD_DETAIL_FILE_HEADER_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstdint>

namespace ffpsd::detail
{
    constexpr std::uint32_t kMaxSidePsd = 30000;
    constexpr std::uint32_t kMaxSidePsb = 300000;

    constexpr std::uint32_t MaxSide(bool is_psb) noexcept
    {
        return is_psb ? kMaxSidePsb : kMaxSidePsd;
    }

    // Section 1, a fixed 26 bytes at the start of every file.
    struct FileHeader
    {
        std::uint16_t version = 1;
        std::uint16_t channel_count = 0;
        std::uint32_t height = 0;
        std::uint32_t width = 0;
        std::uint16_t depth = 0;
        std::uint16_t color_mode = 0;
    };
    FileHeader ParseFileHeader(BigEndianReader& reader);
    void WriteFileHeader(BigEndianWriter& writer, const FileHeader& header);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FILE_HEADER_HPP_
