#include "detail/layer_and_mask/layer_and_mask_info.hpp"

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/layer_info.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kLayers16Key = Fourcc('L', 'r', '1', '6');
        constexpr std::uint32_t kLayers32Key = Fourcc('L', 'r', '3', '2');

        constexpr std::uint32_t DeepLayersKey(std::uint16_t depth) noexcept
        {
            return depth == 16 ? kLayers16Key : depth == 32 ? kLayers32Key : 0;
        }
    } // namespace

    LayerAndMaskInfo
    ParseLayerAndMaskInfo(BigEndianReader& reader, bool is_psb, std::uint16_t depth, std::vector<LayerRecord>& records)
    {
        records.clear();

        const std::uint64_t length = is_psb ? reader.ReadU64() : reader.ReadU32();
        if (length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: layer and mask information claims " + std::to_string(length) + " bytes, only " +
                std::to_string(reader.GetRemaining()) + " left");

        const std::size_t end = reader.Tell() + static_cast<std::size_t>(length);

        LayerAndMaskInfo info;
        if (length == 0)
            return info;

        LayerInfo layers = ParseLayerInfo(reader, is_psb);
        if (reader.Tell() > end)
            throw std::runtime_error("ffpsd: the layer info runs past the layer and mask information");

        // Old writers stop after the layer info.
        if (end - reader.Tell() >= sizeof(std::uint32_t))
            info.global_mask = ParseGlobalLayerMaskInfo(reader, end);

        info.blocks = ParseTaggedBlocks(reader, end, is_psb);

        const std::uint32_t deep_key = DeepLayersKey(depth);
        if (layers.records.empty() && deep_key != 0)
        {
            if (const TaggedBlock* block = GetTaggedBlockByKey(info.blocks, deep_key))
            {
                BigEndianReader body(block->data);
                layers = ParseLayerInfoBody(body, block->data.size(), is_psb);
                info.layers_key = deep_key;
            }
        }

        info.merged_alpha = layers.merged_alpha;
        records = std::move(layers.records);
        return info;
    }

    void WriteLayerAndMaskInfo(
        BigEndianWriter& writer, const LayerAndMaskInfo& info, const std::vector<LayerToWrite>& layers, bool is_psb,
        std::uint16_t depth)
    {
        const std::uint32_t deep_key = DeepLayersKey(depth);

        // Rebuilt from the records: the block read from the file is stale.
        TaggedBlock deep;
        deep.key = deep_key;
        if (deep_key != 0 && !layers.empty())
        {
            BigEndianWriter body;
            WriteLayerInfoBody(body, info.merged_alpha, layers, is_psb);
            deep.data = body.Take();
        }

        const std::size_t length = writer.ReserveLength(is_psb);
        WriteLayerInfo(writer, info.merged_alpha, deep_key != 0 ? std::vector<LayerToWrite>() : layers, is_psb);
        WriteGlobalLayerMaskInfo(writer, info.global_mask);

        // In place of the old block, or first when there was none.
        bool deep_written = deep.data.empty();
        if (!deep_written && GetTaggedBlockByKey(info.blocks, deep_key) == nullptr)
        {
            WriteTaggedBlock(writer, deep, is_psb);
            deep_written = true;
        }
        for (const std::unique_ptr<TaggedBlock>& block : info.blocks)
        {
            if (deep_key == 0 || block->key != deep_key)
                WriteTaggedBlock(writer, *block, is_psb);
            else if (!deep_written)
            {
                deep.signature = block->signature;
                WriteTaggedBlock(writer, deep, is_psb);
                deep_written = true;
            }
        }

        writer.PatchLength(length, is_psb);
    }
} // namespace ffpsd::detail
