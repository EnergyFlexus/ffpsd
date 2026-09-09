#ifndef FFPSD_IMAGE_HPP_
#define FFPSD_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffpsd
{
    // Planar, as PSD stores channels; 16 and 32 bit samples are in native byte order.
    struct Image
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8; // bits per sample: 8, 16 or 32
        std::vector<std::uint8_t> bytes;

        bool IsEmpty() const noexcept
        {
            return width == 0 || height == 0 || channel_count == 0;
        }
        std::size_t GetBytesPerSample() const noexcept
        {
            return depth / 8; // 1 bit bitmaps are not supported
        }
        std::size_t GetSizeBytes() const noexcept
        {
            return std::size_t{width} * height * channel_count * GetBytesPerSample();
        }
    };
} // namespace ffpsd

#endif // FFPSD_IMAGE_HPP_
