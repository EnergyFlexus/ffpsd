#ifndef FFPSD_IMAGE_HPP_
#define FFPSD_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd
{
    // Planar, as PSD stores channels; 16 and 32 bit samples are in native byte order.
    struct Image
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;                // bits per sample: 8, 16 or 32
        ColorMode color_mode = ColorMode::kRgb; // what the planes mean; transparency, when there is one, follows the colors
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

    // An Image's fields over borrowed bytes; a view of a temporary Image dangles after the statement.
    struct ImageView
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;
        ColorMode color_mode = ColorMode::kRgb;
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;

        ImageView() = default;
        // The fields in order; the names differ only so they shadow no member.
        ImageView(
            std::uint32_t view_width, std::uint32_t view_height, std::uint16_t view_channel_count, std::uint16_t view_depth,
            ColorMode view_color_mode, const std::uint8_t* view_data, std::size_t view_size) noexcept
            : width(view_width)
            , height(view_height)
            , channel_count(view_channel_count)
            , depth(view_depth)
            , color_mode(view_color_mode)
            , data(view_data)
            , size(view_size)
        {
        }
        ImageView(const Image& image) noexcept
            : width(image.width)
            , height(image.height)
            , channel_count(image.channel_count)
            , depth(image.depth)
            , color_mode(image.color_mode)
            , data(image.bytes.data())
            , size(image.bytes.size())
        {
        }

        bool IsEmpty() const noexcept
        {
            return width == 0 || height == 0 || channel_count == 0;
        }
        std::size_t GetBytesPerSample() const noexcept
        {
            return depth / 8;
        }
        std::size_t GetSizeBytes() const noexcept
        {
            return std::size_t{width} * height * channel_count * GetBytesPerSample();
        }
    };
} // namespace ffpsd

#endif // FFPSD_IMAGE_HPP_
