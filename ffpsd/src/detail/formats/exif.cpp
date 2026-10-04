#include "detail/formats/exif.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint8_t kExifHeader[] = {'E', 'x', 'i', 'f', 0, 0};
        constexpr std::size_t kTiffHeaderSize = 8;
        constexpr std::size_t kEntrySize = 12;
        constexpr std::uint16_t kOrientationTag = 0x0112;
        constexpr std::uint16_t kShortType = 3;

        // TIFF data in either byte order, "II" or "MM"; every read is bounds checked by the caller.
        struct Tiff
        {
            const std::uint8_t* data = nullptr;
            std::size_t size = 0;
            bool little = false;

            std::uint16_t U16(std::size_t at) const noexcept
            {
                const unsigned first = data[at];
                const unsigned second = data[at + 1];
                return static_cast<std::uint16_t>(little ? first | second << 8 : first << 8 | second);
            }

            std::uint32_t U32(std::size_t at) const noexcept
            {
                const std::uint32_t first = U16(at);
                const std::uint32_t second = U16(at + 2);
                return little ? first | second << 16 : first << 16 | second;
            }
        };
    } // namespace

    Orientation DecodeExifOrientation(const std::uint8_t* data, std::size_t size) noexcept
    {
        if (data == nullptr || size < sizeof(kExifHeader) + kTiffHeaderSize || std::memcmp(data, kExifHeader, sizeof(kExifHeader)) != 0)
            return Orientation::kNormal;

        Tiff tiff;
        tiff.data = data + sizeof(kExifHeader);
        tiff.size = size - sizeof(kExifHeader);
        if (tiff.data[0] == 'I' && tiff.data[1] == 'I')
            tiff.little = true;
        else if (tiff.data[0] != 'M' || tiff.data[1] != 'M')
            return Orientation::kNormal;
        if (tiff.U16(2) != 42)
            return Orientation::kNormal;

        const std::size_t ifd = tiff.U32(4);
        if (ifd > tiff.size - 2)
            return Orientation::kNormal;

        const std::size_t count = tiff.U16(ifd);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::size_t entry = ifd + 2 + i * kEntrySize;
            if (entry + kEntrySize > tiff.size)
                break;
            if (tiff.U16(entry) != kOrientationTag)
                continue;

            // A single SHORT sits in the first two bytes of the value field.
            if (tiff.U16(entry + 2) != kShortType || tiff.U32(entry + 4) != 1)
                return Orientation::kNormal;
            const std::uint16_t value = tiff.U16(entry + 8);
            return value >= 1 && value <= 8 ? static_cast<Orientation>(value) : Orientation::kNormal;
        }
        return Orientation::kNormal;
    }
} // namespace ffpsd::detail
