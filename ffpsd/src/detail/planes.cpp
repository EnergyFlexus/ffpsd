#include "detail/planes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    namespace
    {
        // Where source pixel (x, y) lands in a plane: origin + x * step_x + y * step_y.
        struct Steps
        {
            std::ptrdiff_t origin = 0;
            std::ptrdiff_t step_x = 1;
            std::ptrdiff_t step_y = 0;
        };

        bool SwapsSides(Orientation orientation) noexcept
        {
            return orientation >= Orientation::kTranspose;
        }

        Steps StepsOf(Orientation orientation, std::uint32_t width, std::uint32_t height) noexcept
        {
            const auto w = static_cast<std::ptrdiff_t>(width);
            const auto h = static_cast<std::ptrdiff_t>(height);
            const std::ptrdiff_t out_width = SwapsSides(orientation) ? h : w;
            switch (orientation)
            {
            case Orientation::kMirrorHorizontal:
                return {w - 1, -1, out_width};
            case Orientation::kRotate180:
                return {(h - 1) * out_width + w - 1, -1, -out_width};
            case Orientation::kMirrorVertical:
                return {(h - 1) * out_width, 1, -out_width};
            case Orientation::kTranspose:
                return {0, out_width, 1};
            case Orientation::kRotate90:
                return {h - 1, out_width, -1};
            case Orientation::kTransverse:
                return {(w - 1) * out_width + h - 1, -out_width, -1};
            case Orientation::kRotate270:
                return {(w - 1) * out_width, -out_width, 1};
            default:
                return {0, 1, out_width};
            }
        }

        template <std::size_t kSample>
        void Scatter(
            const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channels, Steps steps,
            std::uint8_t* planes)
        {
            const std::size_t plane_bytes = std::size_t{width} * height * kSample;
            const std::uint8_t* in = pixels;
            for (std::uint32_t y = 0; y < height; ++y)
            {
                std::ptrdiff_t at = steps.origin + static_cast<std::ptrdiff_t>(y) * steps.step_y;
                for (std::uint32_t x = 0; x < width; ++x, at += steps.step_x)
                {
                    std::uint8_t* out = planes + static_cast<std::size_t>(at) * kSample;
                    for (std::uint16_t channel = 0; channel < channels; ++channel, in += kSample)
                        std::memcpy(out + channel * plane_bytes, in, kSample);
                }
            }
        }
    } // namespace

    Image Deinterleave(
        const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        Orientation orientation)
    {
        Image image;
        image.width = SwapsSides(orientation) ? height : width;
        image.height = SwapsSides(orientation) ? width : height;
        image.channel_count = channel_count;
        image.depth = depth;
        image.bytes.resize(image.GetSizeBytes());

        const Steps steps = StepsOf(orientation, width, height);
        switch (image.GetBytesPerSample())
        {
        case 1:
            Scatter<1>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        case 2:
            Scatter<2>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        case 4:
            Scatter<4>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        default:
            throw std::invalid_argument("ffpsd: no planes of " + std::to_string(depth) + " bit samples");
        }
        return image;
    }

    std::vector<std::uint8_t> Interleave(const Image& image, std::uint16_t channel_count)
    {
        const std::size_t sample = image.GetBytesPerSample();
        const std::size_t pixels = std::size_t{image.width} * image.height;
        std::vector<std::uint8_t> out(pixels * channel_count * sample);
        for (std::size_t channel = 0; channel < channel_count; ++channel)
        {
            const std::uint8_t* plane = image.bytes.data() + channel * pixels * sample;
            for (std::size_t i = 0; i < pixels; ++i)
                std::memcpy(out.data() + (i * channel_count + channel) * sample, plane + i * sample, sample);
        }
        return out;
    }
} // namespace ffpsd::detail
