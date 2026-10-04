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

    // Where Document::ResizeCanvas keeps the old canvas; row by row, since it reads value / 3 and value % 3.
    enum class Anchor : std::uint8_t
    {
        kTopLeft = 0,
        kTop = 1,
        kTopRight = 2,
        kLeft = 3,
        kCenter = 4,
        kRight = 5,
        kBottomLeft = 6,
        kBottom = 7,
        kBottomRight = 8
    };

    // Clockwise.
    enum class Rotation : std::uint8_t
    {
        k90 = 0,
        k180 = 1,
        k270 = 2
    };

    // kHorizontal mirrors left and right.
    enum class FlipDirection : std::uint8_t
    {
        kHorizontal = 0,
        kVertical = 1
    };

    // Signed: a layer may extend past the canvas.
    struct Rect
    {
        std::int32_t top = 0;
        std::int32_t left = 0;
        std::int32_t bottom = 0;
        std::int32_t right = 0;

        // 64 bit, so rectangles from a damaged file cannot overflow.
        std::int64_t GetWidth() const noexcept
        {
            return std::int64_t{right} - left;
        }
        std::int64_t GetHeight() const noexcept
        {
            return std::int64_t{bottom} - top;
        }
    };
} // namespace ffpsd

#endif // FFPSD_TYPES_HPP_
