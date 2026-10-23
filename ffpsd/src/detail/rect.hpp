#ifndef FFPSD_DETAIL_RECT_HPP_
#define FFPSD_DETAIL_RECT_HPP_

#include <cstdint>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    class BigEndianReader;
    class BigEndianWriter;

    // As every rectangle in the file: top, left, bottom, right.
    Rect ReadRect(BigEndianReader& reader);
    void WriteRect(BigEndianWriter& writer, const Rect& rect);

    // Throws when an edge leaves the 32 bit range.
    Rect CheckedRect(std::int64_t top, std::int64_t left, std::int64_t bottom, std::int64_t right);

    constexpr Rect CanvasRect(std::uint32_t width, std::uint32_t height) noexcept
    {
        return {0, 0, static_cast<std::int32_t>(height), static_cast<std::int32_t>(width)};
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_RECT_HPP_
