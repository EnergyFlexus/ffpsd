#include "detail/formats/picture.hpp"

#include "detail/image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint8_t kPngSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        constexpr std::uint8_t kJpegSignature[] = {0xFF, 0xD8, 0xFF};

        template <std::size_t N> bool StartsWith(const std::uint8_t* data, std::size_t size, const std::uint8_t (&signature)[N]) noexcept
        {
            return data != nullptr && size >= N && std::memcmp(data, signature, N) == 0;
        }
    } // namespace

    std::optional<Format> FindFormat(const std::uint8_t* data, std::size_t size) noexcept
    {
        if (StartsWith(data, size, kPngSignature))
            return Format::kPng;
        if (StartsWith(data, size, kJpegSignature))
            return Format::kJpeg;
        return std::nullopt;
    }

    void CheckPictureMode(ColorMode color_mode, std::uint16_t depth, const char* format)
    {
        if (color_mode != ColorMode::kGrayscale && color_mode != ColorMode::kRgb)
            throw std::invalid_argument(
                std::string("ffpsd: a ") + format + " holds gray or RGB, not color mode " + std::to_string(static_cast<int>(color_mode)));
        if (depth != 8 && depth != 16)
            throw std::invalid_argument(std::string("ffpsd: a ") + format + " holds 8 or 16 bit, not " + std::to_string(depth));
    }

    void CheckPicture(const ImageView& image, const char* format)
    {
        if (image.IsEmpty())
            throw std::invalid_argument(std::string("ffpsd: an empty image makes no ") + format);
        CheckPictureMode(image.color_mode, image.depth, format);
        CheckImage(image);
        CheckColorChannels(image);
    }
} // namespace ffpsd::detail
