#ifndef FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_

#include "detail/io/fourcc.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/adjustments.hpp>
#include <vector>

namespace ffpsd::detail
{
    constexpr std::uint32_t kLevelsKey = Fourcc('l', 'e', 'v', 'l');

    // The first record_count records: all of the color channels, then each channel.
    LevelsInfo ParseLevels(const std::vector<std::uint8_t>& data, std::size_t record_count);

    // As Photoshop writes it: 29 records, then 'Lvls' with 62 in all; missing records are the identity.
    std::vector<std::uint8_t> EncodeLevels(const LevelsInfo& levels);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_LEVELS_HPP_
