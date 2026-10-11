#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        // Every adjustment the specification lists, interpreted or not.
        constexpr std::uint32_t kAdjustmentKeys[] = {Fourcc('l', 'e', 'v', 'l'), Fourcc('c', 'u', 'r', 'v'), Fourcc('b', 'r', 'i', 't'),
                                                     Fourcc('b', 'l', 'n', 'c'), Fourcc('h', 'u', 'e', '2'), Fourcc('h', 'u', 'e', ' '),
                                                     Fourcc('s', 'e', 'l', 'c'), Fourcc('m', 'i', 'x', 'r'), Fourcc('g', 'r', 'd', 'm'),
                                                     Fourcc('p', 'h', 'f', 'l'), Fourcc('e', 'x', 'p', 'A'), Fourcc('v', 'i', 'b', 'A'),
                                                     Fourcc('t', 'h', 'r', 's'), Fourcc('p', 'o', 's', 't'), Fourcc('n', 'v', 'r', 't'),
                                                     Fourcc('b', 'l', 'w', 'h'), Fourcc('c', 'l', 'r', 'L')};

        // Bit 3 as on every layer, bit 4: the pixels do not affect the appearance.
        constexpr std::uint8_t kAdjustmentFlags = 0x18;

    } // namespace

    std::optional<std::uint32_t> FindAdjustmentKey(const TaggedBlocks& blocks) noexcept
    {
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
        {
            if (std::find(std::begin(kAdjustmentKeys), std::end(kAdjustmentKeys), block->key) != std::end(kAdjustmentKeys))
                return block->key;
        }
        return std::nullopt;
    }

    LayerRecord CreateAdjustmentLayerRecord(const std::string& name, std::unique_ptr<TaggedBlock> settings, ColorMode color_mode)
    {
        // No pixels: each channel is just its compression field, the same in PSD and PSB.
        LayerRecord record = LayerRecord::Empty(name, 0, 0);
        ReplaceLayerPixels(record, ImageView{}, color_mode, false, false);
        record.channels.push_back(ChannelImageData::Empty(kLayerMaskId));
        record.mask_data = MaskData::BlankLayerMask();
        record.mask_data.SetDefaultColor(kLayerMaskId, 255);
        record.flags = kAdjustmentFlags;

        // Photoshop writes the settings first.
        record.blocks.insert(record.blocks.begin(), std::move(settings));
        return record;
    }
} // namespace ffpsd::detail
