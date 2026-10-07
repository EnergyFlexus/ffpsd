#ifndef FFPSD_DETAIL_FORMATS_PNG_HPP_
#define FFPSD_DETAIL_FORMATS_PNG_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/formats.hpp>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd::detail
{
    // libpng behind LoadPng and EncodePng.
    Image DecodePng(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);
    std::vector<std::uint8_t> EncodePng(const ImageView& image, PngCompression compression);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FORMATS_PNG_HPP_
