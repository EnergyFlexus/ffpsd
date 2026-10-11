#include "detail/rect.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <limits>
#include <stdexcept>

namespace ffpsd::detail
{
    Rect ReadRect(BigEndianReader& reader)
    {
        Rect rect;
        rect.top = reader.ReadI32();
        rect.left = reader.ReadI32();
        rect.bottom = reader.ReadI32();
        rect.right = reader.ReadI32();
        return rect;
    }

    void WriteRect(BigEndianWriter& writer, const Rect& rect)
    {
        writer.WriteI32(rect.top);
        writer.WriteI32(rect.left);
        writer.WriteI32(rect.bottom);
        writer.WriteI32(rect.right);
    }

    Rect MakeRect(std::int64_t top, std::int64_t left, std::int64_t bottom, std::int64_t right)
    {
        constexpr std::int64_t kLowest = std::numeric_limits<std::int32_t>::min();
        constexpr std::int64_t kHighest = std::numeric_limits<std::int32_t>::max();
        for (const std::int64_t edge : {top, left, bottom, right})
        {
            if (edge < kLowest || edge > kHighest)
                throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
        }
        return {
            static_cast<std::int32_t>(top), static_cast<std::int32_t>(left), static_cast<std::int32_t>(bottom),
            static_cast<std::int32_t>(right)};
    }
} // namespace ffpsd::detail
