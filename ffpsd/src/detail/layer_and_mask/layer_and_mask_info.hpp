#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_AND_MASK_INFO_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_AND_MASK_INFO_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/layer_and_mask/global_layer_mask_info.hpp"
#include "detail/layer_and_mask/layer_info.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_block.hpp"

#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    // Section 4 around the layers; the records go to the caller.
    struct LayerAndMaskInfo
    {
        // A negative layer count: the first alpha channel of the composite is its transparency.
        bool merged_alpha = false;

        GlobalLayerMaskInfo global_mask;

        // In a 16 or 32 bit document one of them is the 'Lr16' or 'Lr32' with the layers.
        TaggedBlocks blocks;

        // 'Lr16' or 'Lr32' when the layers were read from that block, zero otherwise.
        std::uint32_t layers_key = 0;
    };

    LayerAndMaskInfo
    ParseLayerAndMaskInfo(BigEndianReader& reader, bool is_psb, std::uint16_t depth, std::vector<LayerRecord>& records);

    // A 16 or 32 bit document gets its layers in 'Lr16' or 'Lr32' and an empty layer info.
    void WriteLayerAndMaskInfo(
        BigEndianWriter& writer, const LayerAndMaskInfo& info, const std::vector<LayerToWrite>& layers, bool is_psb,
        std::uint16_t depth);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_AND_MASK_INFO_HPP_
