#ifndef FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IDS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IDS_HPP_

#include <cstdint>

namespace ffpsd::detail
{
    // 0 and up are color; below the transparency are the masks, -2 the layer mask and -3 the real user mask.
    constexpr std::int16_t kTransparencyId = -1;
    constexpr std::int16_t kLayerMaskId = -2;
    constexpr std::int16_t kRealMaskId = -3;
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_CHANNEL_IDS_HPP_
