#ifndef FFPSD_DETAIL_LIMITS_HPP_
#define FFPSD_DETAIL_LIMITS_HPP_

#include <cstdint>
#include <ffpsd/types.hpp>
#include <stdexcept>

// Each Check throws Error: invalid_argument, the default, for a caller's values, runtime_error for a file's.
namespace ffpsd::detail
{
    constexpr std::uint16_t kMaxChannels = 56;

    constexpr std::uint32_t kMaxSidePsd = 30000;
    constexpr std::uint32_t kMaxSidePsb = 300000;

    constexpr std::uint32_t MaxSide(bool is_psb) noexcept
    {
        return is_psb ? kMaxSidePsb : kMaxSidePsd;
    }

    // 1 bit bitmaps and the sample depths.
    constexpr bool IsDepth(std::uint16_t depth) noexcept
    {
        return depth == 1 || depth == 8 || depth == 16 || depth == 32;
    }

    // Whole byte samples; a 1 bit bitmap has none.
    constexpr bool IsSampleDepth(std::uint16_t depth) noexcept
    {
        return depth == 8 || depth == 16 || depth == 32;
    }

    // The modes the format defines; 5 and 6 are none.
    constexpr bool IsColorMode(ColorMode color_mode) noexcept
    {
        switch (color_mode)
        {
        case ColorMode::kBitmap:
        case ColorMode::kGrayscale:
        case ColorMode::kIndexed:
        case ColorMode::kRgb:
        case ColorMode::kCmyk:
        case ColorMode::kMultichannel:
        case ColorMode::kDuotone:
        case ColorMode::kLab:
            return true;
        default:
            return false;
        }
    }

    // A mode the format defines at a depth it has; Bitmap is 1 bit and nothing else is.
    template <class Error = std::invalid_argument> void CheckModeAndDepth(ColorMode color_mode, std::uint16_t depth);

    // 8, 16 or 32 bit, as pixels are held.
    template <class Error = std::invalid_argument> void CheckSampleDepth(std::uint16_t depth);

    // At most kMaxChannels; what names the owner in the message.
    template <class Error = std::invalid_argument> void CheckChannelCount(std::uint32_t count, const char* what);

    // 1 to MaxSide a side.
    template <class Error = std::invalid_argument> void CheckCanvasSides(std::uint32_t width, std::uint32_t height, bool is_psb);

    // A layer may be empty, but no wider or taller than the document could be.
    template <class Error = std::invalid_argument> void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LIMITS_HPP_
