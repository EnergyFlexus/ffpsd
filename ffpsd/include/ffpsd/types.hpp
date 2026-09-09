#ifndef FFPSD_TYPES_HPP_
#define FFPSD_TYPES_HPP_

#include <cstdint>

namespace ffpsd
{
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
