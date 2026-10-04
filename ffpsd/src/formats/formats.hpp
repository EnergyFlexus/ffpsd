#ifndef FFPSD_FORMATS_FORMATS_HPP_
#define FFPSD_FORMATS_FORMATS_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/formats.hpp>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <optional>

namespace ffpsd::detail
{
    // The format the bytes start with, supported by this build or not.
    std::optional<Format> FindFormat(const std::uint8_t* data, std::size_t size) noexcept;

    // What PNG and JPEG hold, gray or RGB at 8 or 16 bit; format names the one in the message.
    void CheckPictureMode(ColorMode color_mode, std::uint16_t depth, const char* format);

    // Not empty, in a mode and depth CheckPictureMode takes, transparency at most, and the bytes for it.
    void CheckPicture(const ImageView& image, const char* format);
} // namespace ffpsd::detail

#endif // FFPSD_FORMATS_FORMATS_HPP_
