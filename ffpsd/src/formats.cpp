#include "detail/formats/jpeg.hpp"
#include "detail/formats/picture.hpp"
#include "detail/formats/png.hpp"
#include "detail/io/file.hpp"

#include <cstddef>
#include <cstdint>
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
    } // namespace

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
            return detail::DecodePng(data, size, color_mode, depth);
#else
            throw std::runtime_error("ffpsd: built without PNG support");
#endif
        }
        if (format == Format::kJpeg)
        {
#if defined(FFPSD_HAS_JPEG)
            return detail::DecodeJpeg(data, size, color_mode, depth, true);
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

#if defined(FFPSD_HAS_PNG)
    Image LoadPng(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth)
    {
        return detail::DecodePng(data, size, color_mode, depth);
    }

    Image LoadPng(const std::string& path, ColorMode color_mode, std::uint16_t depth)
    {
        const std::vector<std::uint8_t> data = detail::ReadFile(path);
        return detail::DecodePng(data.data(), data.size(), color_mode, depth);
    }

    std::vector<std::uint8_t> EncodePng(const ImageView& image)
    {
        return detail::EncodePng(image);
    }

    void SavePng(const ImageView& image, const std::string& path)
    {
        detail::WriteFile(path, detail::EncodePng(image));
    }
#endif

#if defined(FFPSD_HAS_JPEG)
    Image LoadJpeg(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth, bool apply_orientation)
    {
        return detail::DecodeJpeg(data, size, color_mode, depth, apply_orientation);
    }

    Image LoadJpeg(const std::string& path, ColorMode color_mode, std::uint16_t depth, bool apply_orientation)
    {
        const std::vector<std::uint8_t> data = detail::ReadFile(path);
        return detail::DecodeJpeg(data.data(), data.size(), color_mode, depth, apply_orientation);
    }

    std::vector<std::uint8_t> EncodeJpeg(const ImageView& image, int quality)
    {
        return detail::EncodeJpeg(image, quality);
    }

    void SaveJpeg(const ImageView& image, const std::string& path, int quality)
    {
        detail::WriteFile(path, detail::EncodeJpeg(image, quality));
    }
#endif
} // namespace ffpsd
