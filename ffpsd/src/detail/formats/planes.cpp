#include "detail/formats/planes.hpp"

#include "detail/image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

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

        // A row goes out one plane after another, so the stores run straight and the row stays in cache.
        template <std::size_t kSample, std::uint16_t kChannels>
        void SplitRow(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out)
        {
            for (std::uint16_t channel = 0; channel < kChannels; ++channel, out += plane_bytes)
            {
                for (std::uint32_t x = 0; x < width; ++x)
                    std::memcpy(out + x * kSample, in + (x * kChannels + channel) * kSample, kSample);
            }
        }

        template <std::size_t kSample>
        void SplitRow(const std::uint8_t* in, std::uint32_t width, std::uint16_t channels, std::size_t plane_bytes, std::uint8_t* out)
        {
            switch (channels)
            {
            case 1:
                return SplitRow<kSample, 1>(in, width, plane_bytes, out);
            case 2:
                return SplitRow<kSample, 2>(in, width, plane_bytes, out);
            case 3:
                return SplitRow<kSample, 3>(in, width, plane_bytes, out);
            case 4:
                return SplitRow<kSample, 4>(in, width, plane_bytes, out);
            default:
                for (std::uint16_t channel = 0; channel < channels; ++channel, out += plane_bytes)
                {
                    for (std::uint32_t x = 0; x < width; ++x)
                        std::memcpy(out + x * kSample, in + (std::size_t{x} * channels + channel) * kSample, kSample);
                }
            }
        }
        // SplitRow backwards: a sample size known at compile time makes each copy a plain move instead of a call.
        template <std::size_t kSample, std::uint16_t kChannels>
        void JoinRow(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out) noexcept
        {
            for (std::uint16_t channel = 0; channel < kChannels; ++channel, in += plane_bytes)
            {
                for (std::uint32_t x = 0; x < width; ++x)
                    std::memcpy(out + (x * kChannels + channel) * kSample, in + x * kSample, kSample);
            }
        }

        template <std::size_t kSample>
        void
        JoinRow(const std::uint8_t* in, std::uint32_t width, std::uint16_t channels, std::size_t plane_bytes, std::uint8_t* out) noexcept
        {
            switch (channels)
            {
            case 1:
                return JoinRow<kSample, 1>(in, width, plane_bytes, out);
            case 2:
                return JoinRow<kSample, 2>(in, width, plane_bytes, out);
            case 3:
                return JoinRow<kSample, 3>(in, width, plane_bytes, out);
            case 4:
                return JoinRow<kSample, 4>(in, width, plane_bytes, out);
            default:
                for (std::uint16_t channel = 0; channel < channels; ++channel, in += plane_bytes)
                {
                    for (std::uint32_t x = 0; x < width; ++x)
                        std::memcpy(out + (std::size_t{x} * channels + channel) * kSample, in + x * kSample, kSample);
                }
            }
        }
    } // namespace

    Image MakePlanes(std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth)
    {
        if (!IsSampleDepth(depth))
            throw std::invalid_argument(UnsupportedDepth(depth));

        return MakeImage(width, height, channel_count, depth, channel_count >= 3 ? ColorMode::kRgb : ColorMode::kGrayscale);
    }

    void DeinterleaveRow(const std::uint8_t* row, Image& image, std::uint32_t y)
    {
        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t plane_bytes = row_bytes * image.height;
        std::uint8_t* out = image.bytes.data() + y * row_bytes;
        switch (image.GetBytesPerSample())
        {
        case 1:
            return SplitRow<1>(row, image.width, image.channel_count, plane_bytes, out);
        case 2:
            return SplitRow<2>(row, image.width, image.channel_count, plane_bytes, out);
        default:
            return SplitRow<4>(row, image.width, image.channel_count, plane_bytes, out);
        }
    }

    Image Deinterleave(
        const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        Orientation orientation)
    {
        const bool swaps = SwapsSides(orientation);
        Image image = MakePlanes(swaps ? height : width, swaps ? width : height, channel_count, depth);

        const std::size_t sample = image.GetBytesPerSample();
        if (orientation == Orientation::kNormal)
        {
            for (std::uint32_t y = 0; y < height; ++y)
                DeinterleaveRow(pixels + y * std::size_t{width} * channel_count * sample, image, y);
            return image;
        }

        const Steps steps = StepsOf(orientation, width, height);
        switch (sample)
        {
        case 1:
            Scatter<1>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        case 2:
            Scatter<2>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        default:
            Scatter<4>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        }
        return image;
    }

    void InterleaveRow(const ImageView& image, std::uint16_t channel_count, std::uint32_t y, std::uint8_t* out) noexcept
    {
        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t plane_bytes = row_bytes * image.height;
        const std::uint8_t* in = image.data + y * row_bytes;
        switch (image.GetBytesPerSample())
        {
        case 1:
            return JoinRow<1>(in, image.width, channel_count, plane_bytes, out);
        case 2:
            return JoinRow<2>(in, image.width, channel_count, plane_bytes, out);
        default:
            return JoinRow<4>(in, image.width, channel_count, plane_bytes, out);
        }
    }
} // namespace ffpsd::detail
