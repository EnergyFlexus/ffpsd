#ifndef FFPSD_DETAIL_COLOR_HPP_
#define FFPSD_DETAIL_COLOR_HPP_

#include <cstddef>
#include <ffpsd/document.hpp>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Color channels of a layer in this mode; throws std::invalid_argument for the modes that keep no layers.
    std::size_t LayerColorCount(ColorMode color);

    // Rec. 709 luma of the encoded samples, no color profiles; alpha kept last.
    Image RgbToGray(const Image& rgb);

    // The gray plane three times, alpha kept last.
    Image GrayToRgb(const Image& gray);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_HPP_
