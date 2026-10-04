#include "detail/layer_and_mask/layer_record.hpp"

#include "detail/file_header.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"
#include "detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/protected_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
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

        // Bit 4 of the mask flags: density and feather follow, as the parameter flags pick them.
        constexpr std::uint8_t kMaskHasParameters = 0x10;

        // User mask density and feather, then vector mask density and feather, by bits 0 to 3.
        constexpr std::size_t kMaskParameterSizes[] = {1, 8, 1, 8};

        constexpr std::size_t kRectSize = 16;

        // Bit 3 of the mask flags.
        constexpr std::uint8_t kMaskFromRender = 0x08;
        constexpr std::size_t kMaskDataSize = 20;

        Rect ReadRect(BigEndianReader& reader)
        {
            Rect rect;
            rect.top = reader.ReadI32();
            rect.left = reader.ReadI32();
            rect.bottom = reader.ReadI32();
            rect.right = reader.ReadI32();
            return rect;
        }

        void WriteRect(BigEndianWriter& writer, const Rect& rect)
        {
            writer.WriteI32(rect.top);
            writer.WriteI32(rect.left);
            writer.WriteI32(rect.bottom);
            writer.WriteI32(rect.right);
        }

        std::optional<std::size_t> FindMaskRectOffset(const std::vector<std::uint8_t>& mask_data, std::int16_t id)
        {
            // The layer mask's rectangle, its default color and flags; the size of 20 pads them.
            BigEndianReader reader(mask_data);
            if (reader.GetRemaining() < kRectSize + 2)
                return std::nullopt;
            if (id == kLayerMaskId)
                return 0;
            if (id != kRealMaskId)
                return std::nullopt;

            reader.Skip(kRectSize + 1);
            const std::uint8_t flags = reader.ReadU8();
            if ((flags & kMaskHasParameters) != 0)
            {
                if (reader.GetRemaining() < 1)
                    return std::nullopt;
                const std::uint8_t parameters = reader.ReadU8();
                std::size_t size = 0;
                for (std::size_t bit = 0; bit < std::size(kMaskParameterSizes); ++bit)
                {
                    if ((parameters >> bit & 1) != 0)
                        size += kMaskParameterSizes[bit];
                }
                if (reader.GetRemaining() < size)
                    return std::nullopt;
                reader.Skip(size);
            }

            // The real user mask's flags and background, then its rectangle.
            if (reader.GetRemaining() < 2 + kRectSize)
                return std::nullopt;
            return reader.Tell() + 2;
        }

        // After the layer mask's rectangle, but before the real user mask's.
        std::optional<std::size_t> FindMaskDefaultColorOffset(const std::vector<std::uint8_t>& mask_data, std::int16_t id)
        {
            const std::optional<std::size_t> rect = FindMaskRectOffset(mask_data, id);
            if (!rect.has_value())
                return std::nullopt;
            return id == kLayerMaskId ? *rect + kRectSize : *rect - 1;
        }

    } // namespace

    LayerRecord ParseLayerRecord(BigEndianReader& reader, bool is_psb, std::vector<std::uint64_t>& channel_lengths)
    {
        const std::size_t record_start = reader.Tell();

        LayerRecord record;
        record.bounds = ReadRect(reader);

        const std::uint16_t channel_count = reader.ReadU16();
        if (channel_count > kMaxChannels)
            throw std::runtime_error(
                "ffpsd: layer at offset " + std::to_string(record_start) + " claims " + std::to_string(channel_count) + " channels");

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

        const std::size_t extra_length = reader.CheckLength(reader.ReadU32(), reader.GetSize(), "layer extra data");
        const std::size_t extra_end = reader.Tell() + extra_length;

        record.mask_data = reader.ReadBlob(extra_end, "layer mask data");
        record.blending_ranges = reader.ReadBlob(extra_end, "layer blending ranges");
        record.name = ReadPascalString(reader, kLayerNameAlignment);
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
        writer.WriteBlob(record.mask_data);
        writer.WriteBlob(record.blending_ranges);
        WritePascalString(writer, record.name, kLayerNameAlignment);
        WriteLayerTaggedBlocks(writer, record.blocks, is_psb);
        writer.PatchLength(extra_length, false);
    }

    bool HasBackgroundMarks(const LayerRecord& record) noexcept
    {
        const std::optional<ProtectedSetting> locks = GetTaggedBlock<ProtectedSetting>(record.blocks);
        const bool has_transparency = FindChannel(record, kTransparencyId) != nullptr;
        return (record.flags & kTransparencyLocked) != 0 || (locks.has_value() && locks->flags != 0) || !has_transparency;
    }

    void MarkAsBackground(LayerRecord& record)
    {
        record.flags = static_cast<std::uint8_t>(record.flags | kTransparencyLocked);
        SetTaggedBlock(record.blocks, LayerNameSourceSetting{LayerNameSourceSetting::kBackground});
        SetTaggedBlock(record.blocks, ProtectedSetting{kBackgroundProtection});
    }

    void UnmarkBackground(LayerRecord& record, std::uint16_t depth, bool is_psb)
    {
        record.flags = static_cast<std::uint8_t>(record.flags & ~kTransparencyLocked);
        SetTaggedBlock(record.blocks, LayerNameSourceSetting{LayerNameSourceSetting::kLayer});
        SetTaggedBlock(record.blocks, ProtectedSetting{0});

        const bool has_transparency = FindChannel(record, kTransparencyId) != nullptr;
        if (!has_transparency)
        {
            const auto width = static_cast<std::size_t>(std::max<std::int64_t>(record.bounds.GetWidth(), 0));
            const auto height = static_cast<std::size_t>(std::max<std::int64_t>(record.bounds.GetHeight(), 0));
            record.channels.insert(record.channels.begin(), EncodeOpaqueChannel(kTransparencyId, width, height, depth, is_psb));
        }
    }

    Rect MakeRect(std::int64_t top, std::int64_t left, std::int64_t bottom, std::int64_t right)
    {
        constexpr std::int64_t kLowest = std::numeric_limits<std::int32_t>::min();
        constexpr std::int64_t kHighest = std::numeric_limits<std::int32_t>::max();
        for (const std::int64_t edge : {top, left, bottom, right})
        {
            if (edge < kLowest || edge > kHighest)
                throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
        }
        return {
            static_cast<std::int32_t>(top), static_cast<std::int32_t>(left), static_cast<std::int32_t>(bottom),
            static_cast<std::int32_t>(right)};
    }

    std::optional<Rect> FindMaskBounds(const std::vector<std::uint8_t>& mask_data, std::int16_t id)
    {
        const std::optional<std::size_t> offset = FindMaskRectOffset(mask_data, id);
        if (!offset.has_value())
            return std::nullopt;

        BigEndianReader reader(mask_data);
        reader.Skip(*offset);
        return ReadRect(reader);
    }

    Rect RequireMaskBounds(const std::vector<std::uint8_t>& mask_data, std::int16_t id)
    {
        const std::optional<Rect> bounds = FindMaskBounds(mask_data, id);
        if (!bounds.has_value())
            throw std::runtime_error("ffpsd: the mask data has no rectangle for mask " + std::to_string(id));
        return *bounds;
    }

    void SetMaskBounds(std::vector<std::uint8_t>& mask_data, std::int16_t id, const Rect& bounds)
    {
        const std::optional<std::size_t> offset = FindMaskRectOffset(mask_data, id);
        if (!offset.has_value())
            throw std::runtime_error("ffpsd: the mask data has no rectangle for mask " + std::to_string(id));

        BigEndianWriter writer(kRectSize);
        WriteRect(writer, bounds);
        std::copy(writer.GetBytes().begin(), writer.GetBytes().end(), mask_data.begin() + static_cast<std::ptrdiff_t>(*offset));
    }

    std::optional<std::uint8_t> FindMaskDefaultColor(const std::vector<std::uint8_t>& mask_data, std::int16_t id)
    {
        const std::optional<std::size_t> offset = FindMaskDefaultColorOffset(mask_data, id);
        return offset.has_value() ? std::optional<std::uint8_t>(mask_data[*offset]) : std::nullopt;
    }

    void SetMaskDefaultColor(std::vector<std::uint8_t>& mask_data, std::int16_t id, std::uint8_t color)
    {
        const std::optional<std::size_t> offset = FindMaskDefaultColorOffset(mask_data, id);
        if (!offset.has_value())
            throw std::runtime_error("ffpsd: the mask data has no default color for mask " + std::to_string(id));
        mask_data[*offset] = color;
    }

    bool IsRenderedMask(const std::vector<std::uint8_t>& mask_data) noexcept
    {
        return mask_data.size() >= kRectSize + 2 && (mask_data[kRectSize + 1] & kMaskFromRender) != 0;
    }

    const ChannelImageData* FindChannel(const LayerRecord& record, std::int16_t id) noexcept
    {
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id == id)
                return &channel;
        }
        return nullptr;
    }

    void RemoveChannels(LayerRecord& record, std::int16_t id)
    {
        std::vector<ChannelImageData>& channels = record.channels;
        channels.erase(
            std::remove_if(channels.begin(), channels.end(), [id](const ChannelImageData& channel) { return channel.id == id; }),
            channels.end());
    }

    std::vector<std::uint8_t> NewMaskData()
    {
        return std::vector<std::uint8_t>(kMaskDataSize, 0);
    }

    void SetLayerName(LayerRecord& record, const std::string& name)
    {
        SetTaggedBlock(record.blocks, UnicodeLayerName{name});
        record.name = name;
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
