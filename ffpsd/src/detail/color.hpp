#ifndef FFPSD_DETAIL_COLOR_HPP_
#define FFPSD_DETAIL_COLOR_HPP_

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // Color channels of the mode, alpha channels not counted; throws std::invalid_argument for multichannel.
    std::uint16_t ColorChannelCount(ColorMode color_mode);

    // Rec. 709 luma of the encoded samples, no color profiles; planes after the colors, such as alpha, stay as they are.
    Image RgbToGray(const Image& rgb);

    // The gray plane three times; planes after the colors, such as alpha, stay as they are.
    Image GrayToRgb(const Image& gray);

    // The image as it is in its own mode, otherwise through RgbToGray or GrayToRgb, which refuse anything else.
    Image ConvertColorMode(Image image, ColorMode color_mode);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_HPP_
