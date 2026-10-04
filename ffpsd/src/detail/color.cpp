#include "detail/color.hpp"

#include "detail/image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ffpsd::detail
{
    namespace
    {
        // Rec. 709 weights scaled to 1 << 15; they add up to exactly 32768.
        constexpr std::uint32_t kRedWeight = 6966;
        constexpr std::uint32_t kGreenWeight = 23436;
        constexpr std::uint32_t kBlueWeight = 2366;
        constexpr std::uint32_t kWeightShift = 15;

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

        template <typename T> T Luma(T red, T green, T blue) noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return 0.2126f * red + 0.7152f * green + 0.0722f * blue;
            else
                return static_cast<T>(
                    (kRedWeight * red + kGreenWeight * green + kBlueWeight * blue + (1u << (kWeightShift - 1))) >> kWeightShift);
        }

        template <typename T> void LumaPlane(const std::uint8_t* rgb, std::uint8_t* gray, std::size_t pixels)
        {
            const std::uint8_t* red = rgb;
            const std::uint8_t* green = rgb + pixels * sizeof(T);
            const std::uint8_t* blue = rgb + 2 * pixels * sizeof(T);
            for (std::size_t i = 0; i < pixels; ++i)
            {
                const std::size_t at = i * sizeof(T);
                Store<T>(gray + at, Luma(Load<T>(red + at), Load<T>(green + at), Load<T>(blue + at)));
            }
        }

        // A composite may hold more than one plane past the colors, so only the fewest and the most are fixed.
        void CheckSource(const Image& image, ColorMode color_mode, std::uint16_t most, const char* what)
        {
            const std::uint16_t fewest = ColorChannelCount(color_mode);
            if (image.color_mode != color_mode || image.channel_count < fewest || image.channel_count > most)
                throw std::invalid_argument(
                    std::string("ffpsd: ") + what + " takes color mode " + std::to_string(static_cast<int>(color_mode)) + " with " +
                    std::to_string(fewest) + " to " + std::to_string(most) + " channels, not color mode " +
                    std::to_string(static_cast<int>(image.color_mode)) + " with " + std::to_string(image.channel_count));
            CheckImage(image);
        }

        Image MakeImage(const Image& source, ColorMode color_mode, std::uint16_t channel_count)
        {
            Image result;
            result.width = source.width;
            result.height = source.height;
            result.depth = source.depth;
            result.color_mode = color_mode;
            result.channel_count = channel_count;
            result.bytes.resize(result.GetSizeBytes());
            return result;
        }
    } // namespace

    std::uint16_t ColorChannelCount(ColorMode color_mode)
    {
        switch (color_mode)
        {
        case ColorMode::kBitmap:
        case ColorMode::kGrayscale:
        case ColorMode::kIndexed:
        case ColorMode::kDuotone:
            return 1;
        case ColorMode::kRgb:
        case ColorMode::kLab:
            return 3;
        case ColorMode::kCmyk:
            return 4;
        default:
            throw std::invalid_argument(
                "ffpsd: color mode " + std::to_string(static_cast<int>(color_mode)) + " has no fixed number of color channels");
        }
    }

    Image RgbToGray(const Image& rgb)
    {
        CheckSource(rgb, ColorMode::kRgb, std::numeric_limits<std::uint16_t>::max(), "RGB to gray");
        Image gray = MakeImage(rgb, ColorMode::kGrayscale, static_cast<std::uint16_t>(rgb.channel_count - 2));

        const std::size_t pixels = std::size_t{rgb.width} * rgb.height;
        const std::size_t plane = pixels * rgb.GetBytesPerSample();
        switch (rgb.depth)
        {
        case 8:
            LumaPlane<std::uint8_t>(rgb.bytes.data(), gray.bytes.data(), pixels);
            break;
        case 16:
            LumaPlane<std::uint16_t>(rgb.bytes.data(), gray.bytes.data(), pixels);
            break;
        default:
            LumaPlane<float>(rgb.bytes.data(), gray.bytes.data(), pixels);
            break;
        }

        // Transparency, or a composite's alpha channels.
        const std::size_t extra = std::size_t{rgb.channel_count} - 3;
        if (extra != 0 && plane != 0)
            std::memcpy(gray.bytes.data() + plane, rgb.bytes.data() + 3 * plane, extra * plane);
        return gray;
    }

    Image GrayToRgb(const Image& gray)
    {
        CheckSource(gray, ColorMode::kGrayscale, std::numeric_limits<std::uint16_t>::max() - 2, "gray to RGB");
        Image rgb = MakeImage(gray, ColorMode::kRgb, static_cast<std::uint16_t>(gray.channel_count + 2));

        const std::size_t plane = std::size_t{gray.width} * gray.height * gray.GetBytesPerSample();
        if (plane == 0)
            return rgb;

        for (std::size_t channel = 0; channel < 3; ++channel)
            std::memcpy(rgb.bytes.data() + channel * plane, gray.bytes.data(), plane);

        // Transparency, or a composite's alpha channels.
        const std::size_t extra = std::size_t{gray.channel_count} - 1;
        if (extra != 0)
            std::memcpy(rgb.bytes.data() + 3 * plane, gray.bytes.data() + plane, extra * plane);
        return rgb;
    }

    Image ConvertColorMode(Image image, ColorMode color_mode)
    {
        if (image.color_mode == color_mode)
            return image;
        return color_mode == ColorMode::kGrayscale ? RgbToGray(image) : GrayToRgb(image);
    }
} // namespace ffpsd::detail
