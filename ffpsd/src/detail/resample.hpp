#ifndef FFPSD_DETAIL_RESAMPLE_HPP_
#define FFPSD_DETAIL_RESAMPLE_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>

namespace ffpsd::detail
{
    // Transparency, if any, premultiplies the color; the caller checks the arguments.
    Image Resample(const Image& image, std::uint32_t width, std::uint32_t height, ResampleFilter filter);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_RESAMPLE_HPP_
