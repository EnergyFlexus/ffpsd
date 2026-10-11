#ifndef FFPSD_DETAIL_SAMPLES_HPP_
#define FFPSD_DETAIL_SAMPLES_HPP_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace ffpsd::detail
{
    // A bitmap's bits count as one byte samples, which need no swap.
    constexpr std::size_t SampleBytes(std::uint16_t depth) noexcept
    {
        return depth <= 8 ? 1 : depth / 8u;
    }

    // Bitmap rows are padded to a whole byte.
    constexpr std::size_t RowBytes(std::uint32_t width, std::uint16_t depth) noexcept
    {
        return depth == 1 ? (std::size_t{width} + 7) / 8 : std::size_t{width} * SampleBytes(depth);
    }

    // Through memcpy, so a sample needs no alignment.
    template <typename T> T Load(const std::uint8_t* at) noexcept
    {
        T value;
        std::memcpy(&value, at, sizeof(T));
        return value;
    }

    template <typename T> void Store(std::uint8_t* at, T value) noexcept
    {
        std::memcpy(at, &value, sizeof(T));
    }

    // 1 for float samples, the maximum for integer ones.
    template <typename T> constexpr T Full() noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
            return T{1};
        else
            return std::numeric_limits<T>::max();
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_SAMPLES_HPP_
