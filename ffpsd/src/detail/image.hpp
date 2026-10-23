#ifndef FFPSD_DETAIL_IMAGE_HPP_
#define FFPSD_DETAIL_IMAGE_HPP_

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // 8, 16 or 32 bit, and the bytes the geometry needs.
    void CheckImage(const ImageView& image);

    // Memory a caller hands in for pixels of this shape: exactly its size, as a smaller one would be overrun.
    void CheckBytesSize(const ImageInfo& info, const std::uint8_t* out, std::size_t size);

    // The colors of the mode and at most one plane more, transparency.
    void CheckColorChannels(const ImageView& image);

    // An empty image passes; otherwise the mode, depth, bytes and channels must fit.
    void CheckImageFormat(const ImageView& image, ColorMode color_mode, std::uint16_t depth);

    bool HasTransparency(const ImageView& image);

    // Same channel count and depth; the part of source outside target is cut.
    void PlaceImage(Image& target, const ImageView& source, std::int64_t top, std::int64_t left);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_HPP_
