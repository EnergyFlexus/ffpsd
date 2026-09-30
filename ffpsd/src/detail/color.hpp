#ifndef FFPSD_DETAIL_COLOR_HPP_
#define FFPSD_DETAIL_COLOR_HPP_

#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Color channels of the mode, alpha channels not counted; throws std::invalid_argument for multichannel.
    std::uint16_t ColorChannelCount(ColorMode color_mode);

    // Rec. 709 luma of the encoded samples, no color profiles; planes after the third, such as alpha, stay as they are.
    Image RgbToGray(const Image& rgb);

    // The gray plane three times; planes after it, such as alpha, stay as they are.
    Image GrayToRgb(const Image& gray);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_HPP_
