#ifndef FFPSD_TYPES_HPP_
#define FFPSD_TYPES_HPP_

#include <cstdint>

namespace ffpsd
{
    enum class ColorMode : std::uint16_t
    {
        kBitmap = 0,
        kGrayscale = 1,
        kIndexed = 2,
        kRgb = 3,
        kCmyk = 4,
        kMultichannel = 7,
        kDuotone = 8,
        kLab = 9
    };

    // A PSD row too long for its 2 byte count leaves its channel raw whatever is asked.
    enum class Compression : std::uint16_t
    {
        kRaw = 0,
        kRle = 1,      // even where it comes out larger than raw
        kRleOrRaw = 2, // per channel, whichever is smaller
    };

    // Where Document::ResizeCanvas keeps the old canvas within the new one.
    enum class Anchor : std::uint8_t
    {
        kTopLeft,
        kTop,
        kTopRight,
        kLeft,
        kCenter,
        kRight,
        kBottomLeft,
        kBottom,
        kBottomRight
    };

    // Signed: a layer may extend past the canvas.
    struct Rect
    {
        std::int32_t top = 0;
        std::int32_t left = 0;
        std::int32_t bottom = 0;
        std::int32_t right = 0;

        std::int32_t GetWidth() const noexcept
        {
            return right - left;
        }
        std::int32_t GetHeight() const noexcept
        {
            return bottom - top;
        }
    };
} // namespace ffpsd

#endif // FFPSD_TYPES_HPP_
