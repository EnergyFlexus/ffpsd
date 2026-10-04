#ifndef FFPSD_DETAIL_IMAGE_HPP_
#define FFPSD_DETAIL_IMAGE_HPP_

#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // 8, 16 or 32 bit, and the bytes the geometry needs.
    void CheckImage(const ImageView& image);

    // The colors of the mode and at most one plane more, transparency.
    void CheckColorChannels(const ImageView& image);

    bool HasTransparency(const ImageView& image);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_HPP_
