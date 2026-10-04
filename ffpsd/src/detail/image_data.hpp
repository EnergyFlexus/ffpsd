#ifndef FFPSD_DETAIL_IMAGE_DATA_HPP_
#define FFPSD_DETAIL_IMAGE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/pixel_data.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // Section 5, the merged image, as stored: the rest of the file, every channel's rows in one run.
    PixelData
    ParseImageData(BigEndianReader& reader, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth);

    // Raw and RLE; no data gives an empty Image.
    Image DecodeImageData(
        const PixelData& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        ColorMode color_mode, bool is_psb);

    // RLE when smaller.
    PixelData EncodeImageData(const ImageView& image, bool is_psb);

    // Zeros for a file without a composite; in RLE a large document costs a few bytes a row.
    PixelData EncodeBlankImageData(
        std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, bool is_psb,
        std::uint16_t compression);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_DATA_HPP_
