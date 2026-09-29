#ifndef FFPSD_DETAIL_COMPOSITE_HPP_
#define FFPSD_DETAIL_COMPOSITE_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Paper white: every channel at its maximum, except a and b of Lab, which are neutral in the middle.
    Image WhiteImage(std::uint32_t width, std::uint32_t height, ColorMode color, std::size_t color_count, std::uint16_t depth);

    // Normal blending at top, left, cut to target; a plane past color_count is alpha, scaled by opacity.
    void
    CompositeNormal(Image& target, const Image& source, std::int32_t top, std::int32_t left, std::uint8_t opacity, std::size_t color_count);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COMPOSITE_HPP_
