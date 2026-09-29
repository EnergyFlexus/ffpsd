#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_INFO_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_INFO_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/layer.hpp>
#include <memory>
#include <vector>

namespace ffpsd::detail
{
    // One allocation per layer, so pointers survive inserts, removals and moves.
    using Layers = std::vector<std::unique_ptr<Layer>>;

    // Empty in 16 and 32 bit documents: the layers are in an 'Lr16' or 'Lr32' block.
    struct LayerInfo
    {
        // A negative count: the first alpha channel of the composite is its transparency.
        bool merged_alpha = false;

        // Bottom to top, as the file stores them.
        std::vector<LayerRecord> records;
    };

    // With its own length field; a zero length is an empty layer info.
    LayerInfo ParseLayerInfo(BigEndianReader& reader, bool is_psb, std::uint16_t depth);

    // What follows the length, which is all an 'Lr16' or 'Lr32' block holds.
    LayerInfo ParseLayerInfoBody(BigEndianReader& reader, std::size_t end, bool is_psb, std::uint16_t depth);

    // A record and the data to write for each of its channels.
    struct LayerToWrite
    {
        const LayerRecord* record = nullptr;
        std::vector<const PixelData*> channels;
    };

    // Layers bottom to top; none give an empty layer info, just its length field.
    void WriteLayerInfo(BigEndianWriter& writer, bool merged_alpha, const std::vector<LayerToWrite>& layers, bool is_psb);
    void WriteLayerInfoBody(BigEndianWriter& writer, bool merged_alpha, const std::vector<LayerToWrite>& layers, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_INFO_HPP_
