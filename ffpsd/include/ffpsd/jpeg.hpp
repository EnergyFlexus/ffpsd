#ifndef FFPSD_JPEG_HPP_
#define FFPSD_JPEG_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <string>
#include <vector>

namespace ffpsd
{
    // Gray or RGB at 8 or 16 bit, CMYK converted without color profiles; apply_orientation turns it upright by EXIF.
    FFPSD_EXPORT Image LoadJpeg(const std::string& path, ColorMode color_mode, std::uint16_t depth, bool apply_orientation = true);
    FFPSD_EXPORT Image
    LoadJpeg(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth, bool apply_orientation = true);

    // Gray from 1 or 2 channels, RGB from 3 or 4, alpha dropped and 16 bit narrowed to 8; quality is 1 to 100.
    FFPSD_EXPORT std::vector<std::uint8_t> EncodeJpeg(const Image& image, int quality = 90);
    FFPSD_EXPORT void SaveJpeg(const Image& image, const std::string& path, int quality = 90);
} // namespace ffpsd

#endif // FFPSD_JPEG_HPP_
