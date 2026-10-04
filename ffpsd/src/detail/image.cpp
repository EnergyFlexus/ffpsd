#include "detail/image.hpp"

#include "detail/color.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    void CheckImage(const ImageView& image)
    {
        if (image.depth != 8 && image.depth != 16 && image.depth != 32)
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(image.depth));

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
} // namespace ffpsd::detail
