#include "detail/layer_and_mask/layer_record.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"
#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/protected_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"
#include "detail/limits.hpp"
#include "detail/rect.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        // Bit 0 of the flags; Photoshop sets it on a background and nowhere else by default.
        constexpr std::uint8_t kTransparencyLocked = 0x01;

        // Transparency, position and bit 3, which the specification leaves out, as Photoshop writes it.
        constexpr std::uint32_t kBackgroundProtection = 0x0000000D;

        // The Pascal name with its length byte, padded to it.
        constexpr std::size_t kNameAlignment = 4;

        constexpr std::uint32_t kVectorMaskKeys[] = {Fourcc('v', 'm', 's', 'k'), Fourcc('v', 's', 'm', 's')};

        // Bit 3, written by Photoshop 5.0 and later: Photoshop sets it on every layer.
        constexpr std::uint8_t kFlagsPhotoshop5 = 0x08;

        // Bit 4, valid with bit 3: the pixels do not count, as on both records of a group.
        constexpr std::uint8_t kPixelsIrrelevant = 0x10;

        constexpr std::uint32_t kPassThrough = Fourcc('p', 'a', 's', 's');

        // Black low, black high, white low, white high, twice: source, then destination.
        constexpr std::uint8_t kPassEverything[] = {0, 0, 255, 255, 0, 0, 255, 255};

        // Photoshop writes five pairs whatever the color mode and the channel count.
        constexpr std::size_t kBlendingRangePairs = 5;

        std::vector<std::uint8_t> DefaultBlendingRanges()
        {
            std::vector<std::uint8_t> ranges;
            ranges.reserve(kBlendingRangePairs * sizeof(kPassEverything));
            for (std::size_t i = 0; i < kBlendingRangePairs; ++i)
                ranges.insert(ranges.end(), std::begin(kPassEverything), std::end(kPassEverything));
            return ranges;
        }
    } // namespace

    LayerRecord ParseLayerRecord(BigEndianReader& reader, bool is_psb, std::vector<std::uint64_t>& channel_lengths)
    {
        const std::size_t record_start = reader.Tell();

        LayerRecord record;
        record.bounds = ReadRect(reader);

        const std::uint16_t channel_count = reader.ReadU16();
        CheckChannelCount<std::runtime_error>(channel_count, ("the layer at offset " + std::to_string(record_start)).c_str());

        record.channels.resize(channel_count);
        channel_lengths.clear();
        for (ChannelImageData& channel : record.channels)
        {
            channel.id = reader.ReadI16();
            channel_lengths.push_back(reader.ReadLength(is_psb));
        }

        record.blend_signature = reader.ReadU32();
        if (record.blend_signature != kBlockSignature)
            throw std::runtime_error(
                "ffpsd: expected 8BIM before the blend mode of the layer at offset " + std::to_string(record_start) + ", got '" +
                FourccString(record.blend_signature) + "'");

        record.blend_key = reader.ReadU32();
        record.opacity = reader.ReadU8();
        record.clipping = reader.ReadU8();
        record.flags = reader.ReadU8();
        reader.Skip(1); // filler

        const std::size_t extra_length = reader.CheckedLength(reader.ReadU32(), reader.GetSize(), "layer extra data");
        const std::size_t extra_end = reader.Tell() + extra_length;

        record.mask_data = MaskData(reader.ReadBlob(extra_end, "layer mask data"));
        record.blending_ranges = reader.ReadBlob(extra_end, "layer blending ranges");
        record.name = ReadPascalString(reader, kNameAlignment);
        if (reader.Tell() > extra_end)
            throw std::runtime_error(
                "ffpsd: the name of the layer at offset " + std::to_string(record_start) + " runs past its extra data");

        record.blocks = ParseTaggedBlocks(reader, extra_end, is_psb);
        return record;
    }

    void WriteLayerRecord(BigEndianWriter& writer, const LayerRecord& record, const std::vector<const PixelData*>& channels, bool is_psb)
    {
        WriteRect(writer, record.bounds);

        writer.WriteU16(static_cast<std::uint16_t>(record.channels.size()));
        for (std::size_t i = 0; i < record.channels.size(); ++i)
        {
            writer.WriteI16(record.channels[i].id);
            writer.WriteLength(channels.at(i)->GetBytes().size(), is_psb);
        }

        writer.WriteU32(record.blend_signature);
        writer.WriteU32(record.blend_key);
        writer.WriteU8(record.opacity);
        writer.WriteU8(record.clipping);
        writer.WriteU8(record.flags);
        writer.WriteU8(0); // filler

        const std::size_t extra_length = writer.ReserveLength(false);
        writer.WriteBlob(record.mask_data.GetBytes());
        writer.WriteBlob(record.blending_ranges);
        WritePascalString(writer, record.name, kNameAlignment);
        WriteLayerTaggedBlocks(writer, record.blocks, is_psb);
        writer.PatchLength(extra_length, false);
    }

    LayerRecord LayerRecord::Empty(const std::string& name, std::int32_t top, std::int32_t left)
    {
        LayerRecord record;
        record.bounds = {top, left, top, left};
        record.flags = kFlagsPhotoshop5;
        record.blending_ranges = DefaultBlendingRanges();
        record.SetName(name);
        return record;
    }

    LayerRecord LayerRecord::GroupHeader(const std::string& name)
    {
        LayerRecord record = Empty(name, 0, 0);
        record.flags = kFlagsPhotoshop5 | kPixelsIrrelevant;
        record.blend_key = kPassThrough;
        SectionDividerSetting divider;
        divider.type = SectionDividerSetting::kOpenFolder;
        divider.blend_key = kPassThrough;
        SetTaggedBlock(record.blocks, divider);
        return record;
    }

    // Photoshop gives the end marker no opacity.
    LayerRecord LayerRecord::GroupEnd()
    {
        LayerRecord record = Empty("</Layer group>", 0, 0);
        record.flags = kFlagsPhotoshop5 | kPixelsIrrelevant;
        record.opacity = 0;
        SectionDividerSetting divider;
        divider.type = SectionDividerSetting::kBoundingSectionDivider;
        SetTaggedBlock(record.blocks, divider);
        return record;
    }

    LayerRecord LayerRecord::Copy() const
    {
        LayerRecord copy;
        copy.bounds = bounds;
        copy.blend_signature = blend_signature;
        copy.blend_key = blend_key;
        copy.opacity = opacity;
        copy.clipping = clipping;
        copy.flags = flags;
        copy.mask_data = mask_data;
        copy.blending_ranges = blending_ranges;
        copy.name = name;
        copy.channels = channels;
        copy.blocks = CopyTaggedBlocks(blocks);
        return copy;
    }

    const ChannelImageData* LayerRecord::FindChannel(std::int16_t id) const noexcept
    {
        for (const ChannelImageData& channel : channels)
        {
            if (channel.id == id)
                return &channel;
        }
        return nullptr;
    }

    void LayerRecord::RemoveChannels(std::int16_t id)
    {
        const auto has_id = [id](const ChannelImageData& channel) { return channel.id == id; };
        channels.erase(std::remove_if(channels.begin(), channels.end(), has_id), channels.end());
    }

    bool LayerRecord::HasLayerMask() const noexcept
    {
        for (const ChannelImageData& channel : channels)
        {
            if (channel.id < kTransparencyId)
                return true;
        }
        return false;
    }

    bool LayerRecord::HasVectorMask() const noexcept
    {
        for (const std::uint32_t key : kVectorMaskKeys)
        {
            if (FindTaggedBlock(blocks, key) != nullptr)
                return true;
        }
        return false;
    }

    std::optional<std::int16_t> LayerRecord::FindPixelMaskId() const noexcept
    {
        bool has_layer_mask = false;
        for (const ChannelImageData& channel : channels)
        {
            if (channel.id == kRealMaskId)
                return kRealMaskId;
            has_layer_mask = has_layer_mask || channel.id == kLayerMaskId;
        }
        if (has_layer_mask && !mask_data.IsRendered())
            return kLayerMaskId;
        return std::nullopt;
    }

    void LayerRecord::SetName(const std::string& new_name)
    {
        SetTaggedBlock(blocks, UnicodeLayerName{new_name});
        name = new_name;
    }

    bool LayerRecord::HasBackgroundMarks() const noexcept
    {
        const std::optional<ProtectedSetting> locks = GetTaggedBlock<ProtectedSetting>(blocks);
        const bool has_transparency = FindChannel(kTransparencyId) != nullptr;
        return (flags & kTransparencyLocked) != 0 || (locks.has_value() && locks->flags != 0) || !has_transparency;
    }

    void LayerRecord::MarkAsBackground()
    {
        flags = static_cast<std::uint8_t>(flags | kTransparencyLocked);
        SetTaggedBlock(blocks, LayerNameSourceSetting{LayerNameSourceSetting::kBackground});
        SetTaggedBlock(blocks, ProtectedSetting{kBackgroundProtection});
    }

    void LayerRecord::UnmarkBackground(std::uint16_t depth, bool is_psb)
    {
        flags = static_cast<std::uint8_t>(flags & ~kTransparencyLocked);
        SetTaggedBlock(blocks, LayerNameSourceSetting{LayerNameSourceSetting::kLayer});
        SetTaggedBlock(blocks, ProtectedSetting{0});

        if (FindChannel(kTransparencyId) == nullptr)
        {
            const auto width = static_cast<std::uint32_t>(std::max<std::int64_t>(bounds.GetWidth(), 0));
            const auto height = static_cast<std::uint32_t>(std::max<std::int64_t>(bounds.GetHeight(), 0));
            channels.insert(channels.begin(), ChannelImageData::Opaque(kTransparencyId, width, height, depth, is_psb));
        }
    }
} // namespace ffpsd::detail
