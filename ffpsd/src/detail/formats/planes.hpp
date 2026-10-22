#ifndef FFPSD_DETAIL_FORMATS_PLANES_HPP_
#define FFPSD_DETAIL_FORMATS_PLANES_HPP_

#include "detail/formats/exif.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Planes for interleaved gray or RGB, alpha last, as codecs give them; gray up to two channels, RGB from three.
    Image MakePlanes(std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth);

    // One upright row of interleaved pixels into row y of the planes, while a codec reads the next.
    void DeinterleaveRow(const std::uint8_t* row, Image& image, std::uint32_t y);

    // The whole picture into planes, turned upright in the same pass.
    Image Deinterleave(
        const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        Orientation orientation = Orientation::kNormal);

    // Row y of every plane as interleaved pixels, as codecs take them.
    void InterleaveRow(const ImageView& image, std::uint32_t y, std::uint8_t* out) noexcept;

    // 16 bit samples to 8 for a codec of 8 bit only, rounded as libpng narrows them.
    void NarrowRow(const std::uint8_t* samples, std::size_t count, std::uint8_t* out) noexcept;

    // 8 bit planes to 16 from a codec of 8 bit only, each sample times 257.
    Image WidenTo16(const Image& image);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FORMATS_PLANES_HPP_
