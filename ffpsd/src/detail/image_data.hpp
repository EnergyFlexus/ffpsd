#ifndef FFPSD_DETAIL_IMAGE_DATA_HPP_
#define FFPSD_DETAIL_IMAGE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <vector>

namespace ffpsd::detail
{
    // Section 5, the merged image, as stored: the rest of the file, compression field first.
    std::vector<std::uint8_t> ParseImageData(BigEndianReader& reader);

    // Raw and RLE; no data gives an empty Image.
    Image DecodeImageData(
        const std::vector<std::uint8_t>& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        bool is_psb);

    // RLE when smaller.
    std::vector<std::uint8_t> EncodeImageData(const Image& image, bool is_psb);

    // Zeros for a file without a composite; in RLE a large document costs a few bytes a row.
    std::vector<std::uint8_t> EncodeBlankImageData(
        std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, bool is_psb,
        std::uint16_t compression);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_DATA_HPP_
