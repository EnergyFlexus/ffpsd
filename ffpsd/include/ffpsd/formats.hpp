#ifndef FFPSD_FORMATS_HPP_
#define FFPSD_FORMATS_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <string>
#include <vector>

namespace ffpsd
{
    enum class Format : std::uint8_t
    {
        kPng,
        kJpeg,
        kWebp
    };

    // Speed against size of a written PNG; kFastest can grow flat pictures a few times over.
    enum class PngCompression : std::uint8_t
    {
        kBalanced = 0,
        kSmallest = 1,
        kFastest = 2
    };

    // Whether this build reads and writes the format.
    FFPSD_EXPORT bool IsFormatSupported(Format format) noexcept;

    // PNG, JPEG or WebP by the file's signature, not its name; a JPEG is turned upright by EXIF, as Photoshop opens it.
    FFPSD_EXPORT Image LoadPicture(const std::string& path, ColorMode color_mode, std::uint16_t depth);
    FFPSD_EXPORT Image LoadPicture(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);

#if defined(FFPSD_HAS_PNG)
    // Decodes to what Document::AddLayer takes: gray or RGB, 8 or 16 bit, alpha as the last plane.
    FFPSD_EXPORT Image LoadPng(const std::string& path, ColorMode color_mode, std::uint16_t depth);
    FFPSD_EXPORT Image LoadPng(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);

    // Gray, gray with alpha, RGB or RGBA by the channel count; 8 or 16 bit.
    FFPSD_EXPORT std::vector<std::uint8_t> EncodePng(const ImageView& image, PngCompression compression = PngCompression::kBalanced);
    FFPSD_EXPORT void SavePng(const ImageView& image, const std::string& path, PngCompression compression = PngCompression::kBalanced);
#endif

#if defined(FFPSD_HAS_JPEG)
    // Gray or RGB at 8 or 16 bit, CMYK converted without color profiles; apply_orientation turns it upright by EXIF.
    FFPSD_EXPORT Image LoadJpeg(const std::string& path, ColorMode color_mode, std::uint16_t depth, bool apply_orientation = true);
    FFPSD_EXPORT Image
    LoadJpeg(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth, bool apply_orientation = true);

    // Gray from 1 or 2 channels, RGB from 3 or 4, alpha dropped and 16 bit narrowed to 8; quality is 1 to 100.
    FFPSD_EXPORT std::vector<std::uint8_t> EncodeJpeg(const ImageView& image, int quality = 90);
    FFPSD_EXPORT void SaveJpeg(const ImageView& image, const std::string& path, int quality = 90);
#endif

#if defined(FFPSD_HAS_WEBP)
    // Gray or RGB at 8 or 16 bit, alpha as the last plane; an animated WebP is refused.
    FFPSD_EXPORT Image LoadWebp(const std::string& path, ColorMode color_mode, std::uint16_t depth);
    FFPSD_EXPORT Image LoadWebp(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);

    // RGBA when a pixel is not opaque, RGB otherwise; gray goes to all three colors, 16 bit narrows to 8; quality is 0 to 100.
    // Lossless keeps every 8 bit sample, transparent pixels included, and takes quality as the effort spent on size.
    FFPSD_EXPORT std::vector<std::uint8_t> EncodeWebp(const ImageView& image, int quality = 90, bool lossless = false);
    FFPSD_EXPORT void SaveWebp(const ImageView& image, const std::string& path, int quality = 90, bool lossless = false);
#endif
} // namespace ffpsd

#endif // FFPSD_FORMATS_HPP_
