#include "detail/color.hpp"

#include <cstdint>
#include <cstring>
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

        void CheckImage(const Image& image, std::uint16_t first, std::uint16_t second, const char* what)
        {
            if (image.channel_count != first && image.channel_count != second)
                throw std::invalid_argument(
                    std::string("ffpsd: ") + what + " takes " + std::to_string(first) + " or " + std::to_string(second) +
                    " channels, not " + std::to_string(image.channel_count));
            if (image.depth != 8 && image.depth != 16 && image.depth != 32)
                throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(image.depth));
            if (image.bytes.size() != image.GetSizeBytes())
                throw std::invalid_argument(
                    "ffpsd: image holds " + std::to_string(image.bytes.size()) + " bytes, its geometry needs " +
                    std::to_string(image.GetSizeBytes()));
        }

        Image MakeImage(const Image& source, std::uint16_t channel_count)
        {
            Image result;
            result.width = source.width;
            result.height = source.height;
            result.depth = source.depth;
            result.channel_count = channel_count;
            result.bytes.resize(result.GetSizeBytes());
            return result;
        }
    } // namespace

    std::size_t LayerColorCount(ColorMode color)
    {
        switch (color)
        {
        case ColorMode::kGrayscale:
        case ColorMode::kDuotone:
            return 1;
        case ColorMode::kRgb:
        case ColorMode::kLab:
            return 3;
        case ColorMode::kCmyk:
            return 4;
        default:
            throw std::invalid_argument("ffpsd: color mode " + std::to_string(static_cast<int>(color)) + " has no layers");
        }
    }

    Image RgbToGray(const Image& rgb)
    {
        CheckImage(rgb, 3, 4, "RGB to gray");
        const bool has_alpha = rgb.channel_count == 4;
        Image gray = MakeImage(rgb, has_alpha ? 2 : 1);

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

        if (has_alpha && plane != 0)
            std::memcpy(gray.bytes.data() + plane, rgb.bytes.data() + 3 * plane, plane);
        return gray;
    }

    Image GrayToRgb(const Image& gray)
    {
        CheckImage(gray, 1, 2, "gray to RGB");
        const bool has_alpha = gray.channel_count == 2;
        Image rgb = MakeImage(gray, has_alpha ? 4 : 3);

        const std::size_t plane = std::size_t{gray.width} * gray.height * gray.GetBytesPerSample();
        if (plane == 0)
            return rgb;

        for (std::size_t channel = 0; channel < 3; ++channel)
            std::memcpy(rgb.bytes.data() + channel * plane, gray.bytes.data(), plane);
        if (has_alpha)
            std::memcpy(rgb.bytes.data() + 3 * plane, gray.bytes.data() + plane, plane);
        return rgb;
    }
} // namespace ffpsd::detail
