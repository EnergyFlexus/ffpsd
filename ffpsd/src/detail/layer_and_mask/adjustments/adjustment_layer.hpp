#ifndef FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_ADJUSTMENT_LAYER_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_ADJUSTMENT_LAYER_HPP_

#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // Specialized by each adjustment for its struct; channel_count is the document's.
    template <class T> T DecodeAdjustment(const std::vector<std::uint8_t>& data, std::size_t channel_count);
    template <class T> std::vector<std::uint8_t> EncodeAdjustment(const T& value);

    // The key of the block that makes the layer an adjustment, such as 'levl', or 0.
    std::uint32_t FindAdjustmentKey(const TaggedBlocks& blocks) noexcept;

    // No pixels, empty channels and a white mask, as Photoshop writes it; settings is the adjustment block.
    LayerRecord CreateAdjustmentLayerRecord(const std::string& name, std::unique_ptr<TaggedBlock> settings, std::size_t color_count);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_ADJUSTMENTS_ADJUSTMENT_LAYER_HPP_
