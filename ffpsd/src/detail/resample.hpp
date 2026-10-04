#ifndef FFPSD_DETAIL_RESAMPLE_HPP_
#define FFPSD_DETAIL_RESAMPLE_HPP_

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>

namespace ffpsd::detail
{
    // Transparency premultiplies the color unless planes_alone, for alpha and spot channels; the caller checks the arguments.
    Image Resample(const Image& image, std::uint32_t width, std::uint32_t height, ResampleFilter filter, bool planes_alone);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_RESAMPLE_HPP_
