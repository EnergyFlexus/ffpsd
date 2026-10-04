#ifndef FFPSD_DETAIL_SAMPLES_HPP_
#define FFPSD_DETAIL_SAMPLES_HPP_

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace ffpsd::detail
{
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
