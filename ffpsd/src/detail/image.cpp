#include "detail/image.hpp"

#include "detail/color.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    bool IsSampleDepth(std::uint16_t depth) noexcept
    {
        return depth == 8 || depth == 16 || depth == 32;
    }

    std::string UnsupportedDepth(std::uint16_t depth)
    {
        return "ffpsd: " + std::to_string(depth) + " bit samples are not supported";
    }

    Image MakeImage(std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, ColorMode color_mode)
    {
        Image image;
        image.width = width;
        image.height = height;
        image.channel_count = channel_count;
        image.depth = depth;
        image.color_mode = color_mode;
        image.bytes = Bytes(image.GetSizeBytes(), 0);
        return image;
    }

    void CheckBytesSize(const ImageInfo& info, const std::uint8_t* out, std::size_t size)
    {
        if (size != info.GetSizeBytes())
            throw std::invalid_argument(
                "ffpsd: " + std::to_string(size) + " bytes given for pixels of " + std::to_string(info.GetSizeBytes()));
        if (size != 0 && out == nullptr)
            throw std::invalid_argument("ffpsd: no memory given for the pixels");
    }

    void CheckImage(const ImageView& image)
    {
        if (!IsSampleDepth(image.depth))
            throw std::invalid_argument(UnsupportedDepth(image.depth));

        const std::size_t needed = image.GetSizeBytes();
        if ((image.data == nullptr && needed != 0) || image.size != needed)
            throw std::invalid_argument(
                "ffpsd: the image holds " + std::to_string(image.size) + " bytes, its geometry needs " + std::to_string(needed));
    }

    void CheckColorChannels(const ImageView& image)
    {
        const std::uint16_t colors = ColorChannelCount(image.color_mode);
        if (image.channel_count != colors && image.channel_count != colors + 1)
            throw std::invalid_argument(
                "ffpsd: color mode " + std::to_string(static_cast<int>(image.color_mode)) + " has " + std::to_string(colors) +
                " color channels, the image has " + std::to_string(image.channel_count));
    }

    bool HasTransparency(const ImageView& image)
    {
        return image.channel_count > ColorChannelCount(image.color_mode);
    }

    void PlaceImage(const ImageView& source, Image& target, std::int64_t top, std::int64_t left)
    {
        const std::int64_t first_row = std::max<std::int64_t>(top, 0);
        const std::int64_t last_row = std::min<std::int64_t>(top + source.height, target.height);
        const std::int64_t first_column = std::max<std::int64_t>(left, 0);
        const std::int64_t last_column = std::min<std::int64_t>(left + source.width, target.width);
        if (first_row >= last_row || first_column >= last_column)
            return;

        const std::size_t sample = target.GetBytesPerSample();
        const std::size_t source_plane = std::size_t{source.width} * source.height * sample;
        const std::size_t target_plane = std::size_t{target.width} * target.height * sample;
        const auto run = static_cast<std::size_t>(last_column - first_column) * sample;
        for (std::size_t channel = 0; channel < target.channel_count; ++channel)
        {
            for (std::int64_t y = first_row; y < last_row; ++y)
            {
                const auto from = static_cast<std::size_t>((y - top) * source.width + (first_column - left));
                const auto to = static_cast<std::size_t>(y * target.width + first_column);
                std::memcpy(
                    target.bytes.data() + channel * target_plane + to * sample, source.data + channel * source_plane + from * sample, run);
            }
        }
    }
} // namespace ffpsd::detail
