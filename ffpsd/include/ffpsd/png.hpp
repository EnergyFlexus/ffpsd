#ifndef FFPSD_PNG_HPP_
#define FFPSD_PNG_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <string>
#include <vector>

namespace ffpsd
{
    // Decodes to what Document::AddLayer takes: gray or RGB, 8 or 16 bit, alpha as the last plane.
    FFPSD_EXPORT Image LoadPng(const std::string& path, ColorMode color_mode, std::uint16_t depth);
    FFPSD_EXPORT Image LoadPng(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth);

    // Gray, gray with alpha, RGB or RGBA by the channel count; 8 or 16 bit.
    FFPSD_EXPORT std::vector<std::uint8_t> EncodePng(const Image& image);
    FFPSD_EXPORT void SavePng(const Image& image, const std::string& path);
} // namespace ffpsd

#endif // FFPSD_PNG_HPP_
