#ifndef FFPSD_DETAIL_SIGNATURES_HPP_
#define FFPSD_DETAIL_SIGNATURES_HPP_

#include "detail/io/fourcc.hpp"

#include <cstdint>

namespace ffpsd::detail
{
    // What resources and tagged blocks start with, and the blend mode field of a layer record.
    constexpr std::uint32_t kBlockSignature = Fourcc('8', 'B', 'I', 'M');
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_SIGNATURES_HPP_
