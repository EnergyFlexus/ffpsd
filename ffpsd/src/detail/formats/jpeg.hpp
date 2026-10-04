#ifndef FFPSD_DETAIL_FORMATS_JPEG_HPP_
#define FFPSD_DETAIL_FORMATS_JPEG_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd::detail
{
    // libjpeg behind LoadJpeg and EncodeJpeg.
    Image DecodeJpeg(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth, bool apply_orientation);
    std::vector<std::uint8_t> EncodeJpeg(const ImageView& image, int quality);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FORMATS_JPEG_HPP_
