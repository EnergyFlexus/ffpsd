#ifndef FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/pixel_data.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // 0 and up are color; below the transparency are the masks, -2 the layer mask and -3 the real user mask.
    constexpr std::int16_t kTransparencyId = -1;
    constexpr std::int16_t kLayerMaskId = -2;

    struct ChannelImageData
    {
        std::int16_t id = 0;

        // Kept as stored: reordering needs no decoding.
        PixelData data;
    };

    // Pass two of the layer info; a mask covers its own rectangle, which is not read, so its size is unknown.
    ChannelImageData ParseChannelImageData(
        BigEndianReader& reader, std::size_t end, std::int16_t id, std::uint64_t length, const Rect& bounds, std::uint16_t depth);

    // Planar native samples, RLE when smaller; no samples give just the compression field.
    ChannelImageData EncodeChannelImageData(
        std::int16_t id, const std::uint8_t* samples, std::size_t width, std::size_t height, std::size_t bytes_per_sample, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_
