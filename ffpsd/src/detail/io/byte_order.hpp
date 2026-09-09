#ifndef FFPSD_DETAIL_IO_BYTE_ORDER_HPP_
#define FFPSD_DETAIL_IO_BYTE_ORDER_HPP_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#include <cstdlib>
#define FFPSD_BSWAP16 _byteswap_ushort
#define FFPSD_BSWAP32 _byteswap_ulong
#define FFPSD_BSWAP64 _byteswap_uint64
#else
#define FFPSD_BSWAP16 __builtin_bswap16
#define FFPSD_BSWAP32 __builtin_bswap32
#define FFPSD_BSWAP64 __builtin_bswap64
#endif

namespace ffpsd::detail
{
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    constexpr bool kNativeLittle = true;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    constexpr bool kNativeLittle = false;
#else
#error unknown byte order
#endif

    template <std::size_t N> struct RawUint;
    template <> struct RawUint<1>
    {
        using type = std::uint8_t;
    };
    template <> struct RawUint<2>
    {
        using type = std::uint16_t;
    };
    template <> struct RawUint<4>
    {
        using type = std::uint32_t;
    };
    template <> struct RawUint<8>
    {
        using type = std::uint64_t;
    };
    template <std::size_t N> using RawUintT = typename RawUint<N>::type;

    // Converts between big endian and native order.
    template <typename T> T Swapped(T v) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
        if constexpr (kNativeLittle && sizeof(T) > 1)
        {
            RawUintT<sizeof(T)> raw;
            std::memcpy(&raw, &v, sizeof(T));
            if constexpr (sizeof(T) == 2)
                raw = FFPSD_BSWAP16(raw);
            else if constexpr (sizeof(T) == 4)
                raw = FFPSD_BSWAP32(raw);
            else
                raw = FFPSD_BSWAP64(raw);
            std::memcpy(&v, &raw, sizeof(T));
            return v;
        }
        else
        {
            return v;
        }
    }
} // namespace ffpsd::detail

#undef FFPSD_BSWAP16
#undef FFPSD_BSWAP32
#undef FFPSD_BSWAP64
#endif // FFPSD_DETAIL_IO_BYTE_ORDER_HPP_
