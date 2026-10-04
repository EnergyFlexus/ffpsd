#ifndef FFPSD_DETAIL_FORMATS_EXIF_HPP_
#define FFPSD_DETAIL_FORMATS_EXIF_HPP_

#include <cstddef>
#include <cstdint>

namespace ffpsd::detail
{
    // EXIF's Orientation tag: what turns the stored pixels upright; the rotations are clockwise.
    enum class Orientation : std::uint16_t
    {
        kNormal = 1,
        kMirrorHorizontal = 2,
        kRotate180 = 3,
        kMirrorVertical = 4,
        kTranspose = 5,
        kRotate90 = 6,
        kTransverse = 7,
        kRotate270 = 8
    };

    // The tag of an APP1 payload that starts with "Exif\0\0"; normal when there is none or the data is broken.
    Orientation DecodeExifOrientation(const std::uint8_t* data, std::size_t size) noexcept;
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FORMATS_EXIF_HPP_
