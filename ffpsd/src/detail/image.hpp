#ifndef FFPSD_DETAIL_IMAGE_HPP_
#define FFPSD_DETAIL_IMAGE_HPP_

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <string>

namespace ffpsd::detail
{
    // Whole byte samples; a 1 bit bitmap has none.
    bool IsSampleDepth(std::uint16_t depth) noexcept;
    std::string UnsupportedDepth(std::uint16_t depth);

    // Zeroed samples.
    Image MakeImage(std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, ColorMode color_mode);

    // 8, 16 or 32 bit, and the bytes the geometry needs.
    void CheckImage(const ImageView& image);

    // Memory a caller hands in for pixels of this shape: exactly its size, as a smaller one would be overrun.
    void CheckBytesSize(const ImageInfo& info, const std::uint8_t* out, std::size_t size);

    // The colors of the mode and at most one plane more, transparency.
    void CheckColorChannels(const ImageView& image);

    bool HasTransparency(const ImageView& image);

    // Same channel count and depth; the part of source outside target is cut.
    void PlaceImage(const ImageView& source, Image& target, std::int64_t top, std::int64_t left);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_HPP_
