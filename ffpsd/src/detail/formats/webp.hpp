#ifndef FFPSD_DETAIL_FORMATS_WEBP_HPP_
#define FFPSD_DETAIL_FORMATS_WEBP_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd::detail
{
    Image DecodeWebp(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);
    std::vector<std::uint8_t> EncodeWebp(const ImageView& image, int quality, bool lossless);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FORMATS_WEBP_HPP_
