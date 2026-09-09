#ifndef FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    struct ChannelImageData
    {
        // 0 and up color, -1 transparency, -2 layer mask, -3 real user mask.
        std::int16_t id = 0;

        // Compression field, RLE counts and rows, kept whole: reordering needs no decoding.
        std::vector<std::uint8_t> raw;
    };

    // Pass two of the layer info: the blob a record declared, not past end.
    void
    ReadChannelImageData(BigEndianReader& reader, std::size_t end, std::uint64_t length, ChannelImageData& channel);

    // Planar native samples, RLE when smaller; no samples give just the compression field.
    ChannelImageData EncodeChannel(
        std::int16_t id, const std::uint8_t* samples, std::size_t width, std::size_t height,
        std::size_t bytes_per_sample, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IMAGE_DATA_HPP_
