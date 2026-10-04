#ifndef FFPSD_FORMATS_PLANES_HPP_
#define FFPSD_FORMATS_PLANES_HPP_

#include "formats/exif.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <vector>

namespace ffpsd::detail
{
    // Interleaved gray or RGB, alpha last, as codecs give them, into one plane per channel, turned upright in the same pass.
    Image Deinterleave(
        const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        Orientation orientation = Orientation::kNormal);

    // The first channel_count planes of the image as interleaved pixels, as codecs take them.
    std::vector<std::uint8_t> Interleave(const ImageView& image, std::uint16_t channel_count);
} // namespace ffpsd::detail

#endif // FFPSD_FORMATS_PLANES_HPP_
