#include "detail/limits.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    template <class Error> void CheckModeAndDepth(ColorMode color_mode, std::uint16_t depth)
    {
        if (!IsColorMode(color_mode))
            throw Error("ffpsd: unknown color mode " + std::to_string(static_cast<int>(color_mode)));
        if (!IsDepth(depth))
            throw Error("ffpsd: unsupported depth: " + std::to_string(depth));
        if ((color_mode == ColorMode::kBitmap) != (depth == 1))
            throw Error(
                "ffpsd: only a bitmap is 1 bit, and it is nothing else; not color mode " + std::to_string(static_cast<int>(color_mode)) +
                " at " + std::to_string(depth) + " bit");
    }

    template <class Error> void CheckSampleDepth(std::uint16_t depth)
    {
        if (!IsSampleDepth(depth))
            throw Error("ffpsd: unsupported sample depth: " + std::to_string(depth));
    }

    template <class Error> void CheckChannelCount(std::uint32_t count, const char* what)
    {
        if (count > kMaxChannels)
            throw Error(
                std::string("ffpsd: ") + what + " has " + std::to_string(count) + " channels, at most " + std::to_string(kMaxChannels));
    }

    template <class Error> void CheckCanvasSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width < 1 || width > max_side || height < 1 || height > max_side)
            throw Error(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " canvas, sides are 1 to " +
                std::to_string(max_side));
    }

    template <class Error> void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width > max_side || height > max_side)
            throw Error(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " layer exceeds " + std::to_string(max_side) +
                " pixels a side");
    }

    // The two errors the checks throw; any other type fails to link.
    template void CheckModeAndDepth<std::invalid_argument>(ColorMode, std::uint16_t);
    template void CheckModeAndDepth<std::runtime_error>(ColorMode, std::uint16_t);
    template void CheckSampleDepth<std::invalid_argument>(std::uint16_t);
    template void CheckSampleDepth<std::runtime_error>(std::uint16_t);
    template void CheckChannelCount<std::invalid_argument>(std::uint32_t, const char*);
    template void CheckChannelCount<std::runtime_error>(std::uint32_t, const char*);
    template void CheckCanvasSides<std::invalid_argument>(std::uint32_t, std::uint32_t, bool);
    template void CheckCanvasSides<std::runtime_error>(std::uint32_t, std::uint32_t, bool);
    template void CheckLayerSides<std::invalid_argument>(std::uint32_t, std::uint32_t, bool);
    template void CheckLayerSides<std::runtime_error>(std::uint32_t, std::uint32_t, bool);
} // namespace ffpsd::detail
