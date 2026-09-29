#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fourcc.hpp"

#include <algorithm>
#include <iterator>
#include <memory>

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

        constexpr std::int16_t kLayerMaskId = -2;

        // Bit 3 as on every layer, bit 4: the pixels do not affect the appearance.
        constexpr std::uint8_t kAdjustmentFlags = 0x18;

        // An empty rectangle, a default color of 255, flags 0 and 2 bytes of padding.
        std::vector<std::uint8_t> WhiteMaskData()
        {
            BigEndianWriter writer;
            writer.WriteZeros(16);
            writer.WriteU8(255);
            writer.WriteU8(0);
            writer.WriteZeros(2);
            return writer.Take();
        }
    } // namespace

    std::uint32_t FindAdjustmentKey(const TaggedBlocks& blocks) noexcept
    {
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
        {
            if (std::find(std::begin(kAdjustmentKeys), std::end(kAdjustmentKeys), block->key) != std::end(kAdjustmentKeys))
                return block->key;
        }
        return 0;
    }

    LayerRecord CreateAdjustmentLayerRecord(const std::string& name, std::unique_ptr<TaggedBlock> settings, std::size_t color_count)
    {
        // No pixels: each channel is just its compression field, the same in PSD and PSB.
        LayerRecord record = CreateLayerRecord(name, SamplesView(), 0, 0, color_count, false);
        record.channels.push_back(EncodeChannel(kLayerMaskId, nullptr, 0, 0, 1, false));
        record.mask_data = WhiteMaskData();
        record.flags = kAdjustmentFlags;

        // Photoshop writes the settings first.
        record.blocks.insert(record.blocks.begin(), std::move(settings));
        return record;
    }
} // namespace ffpsd::detail
