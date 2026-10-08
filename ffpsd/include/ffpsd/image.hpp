#ifndef FFPSD_IMAGE_HPP_
#define FFPSD_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/bytes.hpp>
#include <ffpsd/types.hpp>

namespace ffpsd
{
    // What planar pixels are, without them: the part Image and ImageView share. Never delete either through a pointer to it.
    struct ImageInfo
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;                // bits per sample: 8, 16 or 32
        ColorMode color_mode = ColorMode::kRgb; // what the planes mean; transparency, when there is one, follows the colors

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

    // Planar, as PSD stores channels; 16 and 32 bit samples are in native byte order.
    struct Image : ImageInfo
    {
        Bytes bytes;

        Image() = default;
        // Room for every sample, not yet written.
        explicit Image(const ImageInfo& info)
            : ImageInfo(info)
            , bytes(info.GetSizeBytes())
        {
        }
    };

    // An Image's fields over borrowed bytes; a view of a temporary Image dangles after the statement.
    struct ImageView : ImageInfo
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;

        ImageView() = default;
        // The fields in order; the names differ only so they shadow no member.
        ImageView(
            std::uint32_t view_width, std::uint32_t view_height, std::uint16_t view_channel_count, std::uint16_t view_depth,
            ColorMode view_color_mode, const std::uint8_t* view_data, std::size_t view_size) noexcept
            : ImageInfo{view_width, view_height, view_channel_count, view_depth, view_color_mode}
            , data(view_data)
            , size(view_size)
        {
        }
        ImageView(const ImageInfo& info, const std::uint8_t* view_data, std::size_t view_size) noexcept
            : ImageInfo(info)
            , data(view_data)
            , size(view_size)
        {
        }
        ImageView(const Image& image) noexcept
            : ImageInfo(image)
            , data(image.bytes.data())
            , size(image.bytes.size())
        {
        }
    };
} // namespace ffpsd

#endif // FFPSD_IMAGE_HPP_
