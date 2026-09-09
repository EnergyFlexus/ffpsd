#ifndef FFPSD_DETAIL_COLOR_HPP_
#define FFPSD_DETAIL_COLOR_HPP_

#include <cstddef>
#include <ffpsd/document.hpp>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Color channels of a layer in this mode; zero for the modes Photoshop keeps no layers in.
    std::size_t LayerColorChannels(ColorMode color) noexcept;

    // Rec. 709 luma of the encoded samples, alpha kept last; Photoshop converts through profiles.
    Image RgbToGray(const Image& rgb);

    // The gray plane three times, alpha kept last.
    Image GrayToRgb(const Image& gray);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_HPP_
