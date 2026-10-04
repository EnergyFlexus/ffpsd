#ifndef FFPSD_DETAIL_COMPOSITE_HPP_
#define FFPSD_DETAIL_COMPOSITE_HPP_

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // Paper white: every channel at its maximum, except a and b of Lab, which are neutral in the middle.
    Image MakeWhiteImage(std::uint32_t width, std::uint32_t height, ColorMode color_mode, std::uint16_t depth);

    // Normal blending at top, left, cut to target; the source's transparency is scaled by opacity.
    void BlendNormal(Image& target, const ImageView& source, std::int32_t top, std::int32_t left, std::uint8_t opacity);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COMPOSITE_HPP_
