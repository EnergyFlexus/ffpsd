#include "detail/composite.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ffpsd::detail
{
    namespace
    {
        template <typename T> T Load(const std::uint8_t* at) noexcept
        {
            T value;
            std::memcpy(&value, at, sizeof(T));
            return value;
        }

        template <typename T> void Store(std::uint8_t* at, T value) noexcept
        {
            std::memcpy(at, &value, sizeof(T));
        }

        template <typename T> constexpr T Full() noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return T{1};
            else
                return std::numeric_limits<T>::max();
        }

        // Integer samples round to nearest, so a fully opaque source comes through exactly.
        template <typename T> T Mix(T source, T target, T alpha) noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                return source * alpha + target * (T{1} - alpha);
            }
            else
            {
                const std::uint64_t full = Full<T>();
                return static_cast<T>((std::uint64_t{source} * alpha + std::uint64_t{target} * (full - alpha) + full / 2) / full);
            }
        }

        template <typename T> T ScaleByOpacity(T alpha, std::uint8_t opacity) noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::clamp(alpha, T{0}, T{1}) * static_cast<T>(opacity) / T{255};
            else
                return static_cast<T>((std::uint64_t{alpha} * opacity + 127) / 255);
        }

        template <typename T> void Fill(Image& image, std::size_t channel, T value)
        {
            const std::size_t plane = std::size_t{image.width} * image.height;
            std::uint8_t* first = image.bytes.data() + channel * plane * sizeof(T);
            for (std::size_t i = 0; i < plane; ++i)
                Store<T>(first + i * sizeof(T), value);
        }

        // The neutral a and b of Lab: 128 of 255, 32768 of 65535.
        template <typename T> constexpr T Middle() noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return T{0.5};
            else
                return static_cast<T>(Full<T>() / 2 + 1);
        }

        template <typename T> void WhiteChannels(Image& image, ColorMode color_mode)
        {
            const T middle = Middle<T>();
            for (std::size_t channel = 0; channel < image.channel_count; ++channel)
                Fill<T>(image, channel, color_mode == ColorMode::kLab && channel > 0 ? middle : Full<T>());
        }

        template <typename T>
        void
        Composite(Image& target, const Image& source, std::int32_t top, std::int32_t left, std::uint8_t opacity, std::size_t color_count)
        {
            const bool has_alpha = source.channel_count > color_count;
            const std::size_t source_plane = std::size_t{source.width} * source.height;
            const std::size_t target_plane = std::size_t{target.width} * target.height;

            const std::int64_t x_first = std::max<std::int64_t>(0, left);
            const std::int64_t y_first = std::max<std::int64_t>(0, top);
            const std::int64_t x_end = std::min<std::int64_t>(target.width, std::int64_t{left} + source.width);
            const std::int64_t y_end = std::min<std::int64_t>(target.height, std::int64_t{top} + source.height);

            const std::uint8_t* from = source.bytes.data();
            std::uint8_t* to = target.bytes.data();
            for (std::int64_t y = y_first; y < y_end; ++y)
            {
                for (std::int64_t x = x_first; x < x_end; ++x)
                {
                    const auto at_source = static_cast<std::size_t>((y - top) * source.width + (x - left));
                    const auto at_target = static_cast<std::size_t>(y * target.width + x);

                    const T alpha = has_alpha ? Load<T>(from + (color_count * source_plane + at_source) * sizeof(T)) : Full<T>();
                    const T coverage = ScaleByOpacity(alpha, opacity);
                    for (std::size_t c = 0; c < color_count; ++c)
                    {
                        std::uint8_t* sample = to + (c * target_plane + at_target) * sizeof(T);
                        const T over = Load<T>(from + (c * source_plane + at_source) * sizeof(T));
                        Store<T>(sample, Mix(over, Load<T>(sample), coverage));
                    }
                }
            }
        }
    } // namespace

    Image MakeWhiteImage(std::uint32_t width, std::uint32_t height, ColorMode color_mode, std::size_t color_count, std::uint16_t depth)
    {
        Image image;
        image.width = width;
        image.height = height;
        image.channel_count = static_cast<std::uint16_t>(color_count);
        image.depth = depth;
        image.bytes.resize(image.GetSizeBytes());

        switch (depth)
        {
        case 8:
            WhiteChannels<std::uint8_t>(image, color_mode);
            break;
        case 16:
            WhiteChannels<std::uint16_t>(image, color_mode);
            break;
        case 32:
            WhiteChannels<float>(image, color_mode);
            break;
        default:
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(depth));
        }
        return image;
    }

    void BlendNormal(Image& target, const Image& source, std::int32_t top, std::int32_t left, std::uint8_t opacity, std::size_t color_count)
    {
        // An empty layer has nothing to lay down and says nothing about its channels.
        if (source.IsEmpty())
            return;
        if (source.depth != target.depth || target.channel_count != color_count ||
            (source.channel_count != color_count && source.channel_count != color_count + 1) ||
            source.bytes.size() != source.GetSizeBytes() || target.bytes.size() != target.GetSizeBytes())
            throw std::invalid_argument("ffpsd: images that do not composite: channels, depth or size differ");

        switch (target.depth)
        {
        case 8:
            Composite<std::uint8_t>(target, source, top, left, opacity, color_count);
            break;
        case 16:
            Composite<std::uint16_t>(target, source, top, left, opacity, color_count);
            break;
        case 32:
            Composite<float>(target, source, top, left, opacity, color_count);
            break;
        default:
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(target.depth));
        }
    }
} // namespace ffpsd::detail
