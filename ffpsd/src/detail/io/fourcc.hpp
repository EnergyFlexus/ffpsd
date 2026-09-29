#ifndef FFPSD_DETAIL_IO_FOURCC_HPP_
#define FFPSD_DETAIL_IO_FOURCC_HPP_

#include <cstdint>
#include <string>

namespace ffpsd::detail
{
    constexpr std::uint32_t Fourcc(char a, char b, char c, char d) noexcept
    {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) | static_cast<std::uint32_t>(static_cast<unsigned char>(d));
    }

    // Unprintable bytes become '?': a code from a malformed file is arbitrary.
    inline std::string FourccString(std::uint32_t code)
    {
        std::string text(4, '?');
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const auto byte = static_cast<unsigned char>(code >> (8 * (3 - i)));
            if (byte >= 0x20 && byte < 0x7F)
                text[i] = static_cast<char>(byte);
        }
        return text;
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_FOURCC_HPP_
