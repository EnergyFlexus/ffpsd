#ifndef FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_

#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/adjustments.hpp>
#include <vector>

namespace ffpsd::detail
{
    // The first channel_count + 1 records: all of the color channels, then each channel.
    template <> LevelsInfo DecodeAdjustment<LevelsInfo>(const std::vector<std::uint8_t>& data, std::size_t channel_count);

    // As Photoshop writes it: 29 records, then 'Lvls' with 62 in all; missing records are the identity.
    template <> std::vector<std::uint8_t> EncodeAdjustment<LevelsInfo>(const LevelsInfo& levels);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_
