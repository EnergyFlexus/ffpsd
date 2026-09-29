#ifndef FFPSD_DETAIL_COLOR_HPP_
#define FFPSD_DETAIL_COLOR_HPP_

#include <cstddef>
#include <ffpsd/document.hpp>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Color channels of a layer in this mode; throws std::invalid_argument for the modes that keep no layers.
    std::size_t LayerColorCount(ColorMode color_mode);

    // Rec. 709 luma of the encoded samples, no color profiles; planes after the third, such as alpha, stay as they are.
    Image RgbToGray(const Image& rgb);

    // The gray plane three times; planes after it, such as alpha, stay as they are.
    Image GrayToRgb(const Image& gray);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_HPP_
