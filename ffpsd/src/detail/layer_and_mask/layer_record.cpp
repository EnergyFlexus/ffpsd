#include "detail/layer_and_mask/layer_record.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"
#include "detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/protected_setting.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kBlockSignature = Fourcc('8', 'B', 'I', 'M');

        // Bit 0 of the flags; Photoshop sets it on a background and nowhere else by default.
        constexpr std::uint8_t kTransparencyLocked = 0x01;

        // Transparency, position and bit 3, which the specification leaves out, as Photoshop writes it.
        constexpr std::uint32_t kBackgroundProtection = 0x0000000D;
        constexpr std::uint16_t kMaxChannels = 56;

        // A length-prefixed field of the extra data, which must not run past its end.
        std::vector<std::uint8_t> ReadExtraField(BigEndianReader& reader, std::size_t end, const char* what)
        {
            const std::uint32_t length = reader.ReadU32();
            if (reader.Tell() > end || length > end - reader.Tell())
                throw std::runtime_error(
                    std::string("ffpsd: layer ") + what + " claims " + std::to_string(length) + " bytes, more than the extra data holds");

            std::vector<std::uint8_t> field(length);
            if (length != 0)
                reader.ReadU8Array(field.data(), field.size());
            return field;
        }

        void WriteExtraField(BigEndianWriter& writer, const std::vector<std::uint8_t>& field)
        {
            const std::size_t length = writer.ReserveLength(false);
            if (!field.empty())
                writer.WriteU8Array(field.data(), field.size());
            writer.PatchLength(length, false);
        }
    } // namespace

    LayerRecord ParseLayerRecord(BigEndianReader& reader, bool is_psb, std::vector<std::uint64_t>& channel_lengths)
    {
        const std::size_t record_start = reader.Tell();

        LayerRecord record;
        record.bounds.top = reader.ReadI32();
        record.bounds.left = reader.ReadI32();
        record.bounds.bottom = reader.ReadI32();
        record.bounds.right = reader.ReadI32();

        const std::uint16_t channel_count = reader.ReadU16();
        if (channel_count > kMaxChannels)
            throw std::runtime_error(
                "ffpsd: layer at offset " + std::to_string(record_start) + " claims " + std::to_string(channel_count) + " channels");

        record.channels.resize(channel_count);
        channel_lengths.clear();
        for (ChannelImageData& channel : record.channels)
        {
            channel.id = reader.ReadI16();
            channel_lengths.push_back(is_psb ? reader.ReadU64() : reader.ReadU32());
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

        const std::uint32_t extra_length = reader.ReadU32();
        if (extra_length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: layer extra data claims " + std::to_string(extra_length) + " bytes, only " + std::to_string(reader.GetRemaining()) +
                " left");
        const std::size_t extra_end = reader.Tell() + extra_length;

        record.mask_data = ReadExtraField(reader, extra_end, "mask data");
        record.blending_ranges = ReadExtraField(reader, extra_end, "blending ranges");
        record.name = ReadPascalString(reader, kLayerNameAlignment);
        if (reader.Tell() > extra_end)
            throw std::runtime_error(
                "ffpsd: the name of the layer at offset " + std::to_string(record_start) + " runs past its extra data");

        record.blocks = ParseTaggedBlocks(reader, extra_end, is_psb);
        return record;
    }

    void WriteLayerRecord(BigEndianWriter& writer, const LayerRecord& record, const std::vector<const PixelData*>& channels, bool is_psb)
    {
        writer.WriteI32(record.bounds.top);
        writer.WriteI32(record.bounds.left);
        writer.WriteI32(record.bounds.bottom);
        writer.WriteI32(record.bounds.right);

        writer.WriteU16(static_cast<std::uint16_t>(record.channels.size()));
        for (std::size_t i = 0; i < record.channels.size(); ++i)
        {
            const std::size_t size = channels.at(i)->GetBytes().size();
            writer.WriteI16(record.channels[i].id);
            if (is_psb)
                writer.WriteU64(size);
            else if (size > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("ffpsd: a channel of more than 4 GB needs a PSB");
            else
                writer.WriteU32(static_cast<std::uint32_t>(size));
        }

        writer.WriteU32(record.blend_signature);
        writer.WriteU32(record.blend_key);
        writer.WriteU8(record.opacity);
        writer.WriteU8(record.clipping);
        writer.WriteU8(record.flags);
        writer.WriteU8(0); // filler

        const std::size_t extra_length = writer.ReserveLength(false);
        WriteExtraField(writer, record.mask_data);
        WriteExtraField(writer, record.blending_ranges);
        WritePascalString(writer, record.name, kLayerNameAlignment);
        WriteLayerTaggedBlocks(writer, record.blocks, is_psb);
        writer.PatchLength(extra_length, false);
    }

    bool IsBackground(const LayerRecord& record) noexcept
    {
        const std::optional<LayerNameSourceSetting> source = GetTaggedBlock<LayerNameSourceSetting>(record.blocks);
        return source.has_value() && source->id == LayerNameSourceSetting::kBackground;
    }

    void MarkAsBackground(LayerRecord& record)
    {
        record.flags = static_cast<std::uint8_t>(record.flags | kTransparencyLocked);
        SetTaggedBlock(record.blocks, LayerNameSourceSetting{LayerNameSourceSetting::kBackground});
        SetTaggedBlock(record.blocks, ProtectedSetting{kBackgroundProtection});
    }

    void UnmarkBackground(LayerRecord& record)
    {
        record.flags = static_cast<std::uint8_t>(record.flags & ~kTransparencyLocked);
        SetTaggedBlock(record.blocks, LayerNameSourceSetting{LayerNameSourceSetting::kLayer});
        SetTaggedBlock(record.blocks, ProtectedSetting{0});
    }

    LayerRecord CopyLayerRecord(const LayerRecord& source)
    {
        LayerRecord copy;
        copy.bounds = source.bounds;
        copy.blend_signature = source.blend_signature;
        copy.blend_key = source.blend_key;
        copy.opacity = source.opacity;
        copy.clipping = source.clipping;
        copy.flags = source.flags;
        copy.mask_data = source.mask_data;
        copy.blending_ranges = source.blending_ranges;
        copy.name = source.name;
        copy.channels = source.channels;
        copy.blocks = CopyTaggedBlocks(source.blocks);
        return copy;
    }

} // namespace ffpsd::detail
