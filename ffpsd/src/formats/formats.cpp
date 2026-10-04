#include "formats/formats.hpp"

#include "detail/image.hpp"
#include "detail/io/file.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ffpsd/formats.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ffpsd
{
    namespace
    {
#if defined(FFPSD_HAS_PNG)
        constexpr bool kHasPng = true;
#else
        constexpr bool kHasPng = false;
#endif

#if defined(FFPSD_HAS_JPEG)
        constexpr bool kHasJpeg = true;
#else
        constexpr bool kHasJpeg = false;
#endif

        constexpr std::uint8_t kPngSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        constexpr std::uint8_t kJpegSignature[] = {0xFF, 0xD8, 0xFF};

        template <std::size_t N> bool StartsWith(const std::uint8_t* data, std::size_t size, const std::uint8_t (&signature)[N]) noexcept
        {
            return data != nullptr && size >= N && std::memcmp(data, signature, N) == 0;
        }
    } // namespace

    namespace detail
    {
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
                    std::string("ffpsd: a ") + format + " holds gray or RGB, not color mode " +
                    std::to_string(static_cast<int>(color_mode)));
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
    } // namespace detail

    bool IsFormatSupported(Format format) noexcept
    {
        return (format == Format::kPng && kHasPng) || (format == Format::kJpeg && kHasJpeg);
    }

    // A build without either format has no use for the color mode and the depth.
    Image
    LoadPicture(const std::uint8_t* data, std::size_t size, [[maybe_unused]] ColorMode color_mode, [[maybe_unused]] std::uint16_t depth)
    {
        const std::optional<Format> format = detail::FindFormat(data, size);
        if (format == Format::kPng)
        {
#if defined(FFPSD_HAS_PNG)
            return LoadPng(data, size, color_mode, depth);
#else
            throw std::runtime_error("ffpsd: built without PNG support");
#endif
        }
        if (format == Format::kJpeg)
        {
#if defined(FFPSD_HAS_JPEG)
            return LoadJpeg(data, size, color_mode, depth);
#else
            throw std::runtime_error("ffpsd: built without JPEG support");
#endif
        }
        throw std::runtime_error("ffpsd: unsupported image format");
    }

    Image LoadPicture(const std::string& path, ColorMode color_mode, std::uint16_t depth)
    {
        const std::vector<std::uint8_t> data = detail::ReadFile(path);
        return LoadPicture(data.data(), data.size(), color_mode, depth);
    }
} // namespace ffpsd
