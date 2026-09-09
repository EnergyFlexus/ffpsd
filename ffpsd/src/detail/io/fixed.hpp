#ifndef FFPSD_DETAIL_IO_FIXED_HPP_
#define FFPSD_DETAIL_IO_FIXED_HPP_

#include <cmath>
#include <cstdint>

namespace ffpsd::detail
{
    // Adobe's Fixed: signed 16.16.
    inline double FixedToDouble(std::uint32_t raw) noexcept
    {
        return static_cast<double>(static_cast<std::int32_t>(raw)) / 65536.0;
    }

    // The caller rejects values outside the 16.16 range.
    inline std::uint32_t DoubleToFixed(double value) noexcept
    {
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(value * 65536.0)));
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_FIXED_HPP_
