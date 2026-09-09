#include "detail/layer_and_mask/layer_record.hpp"

#include "detail/file_header.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"
#include "detail/pixel_data.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kBlockSignature = Fourcc('8', 'B', 'I', 'M');
        constexpr std::uint32_t kUnicodeNameKey = Fourcc('l', 'u', 'n', 'i');
        constexpr std::uint32_t kLayerIdKey = Fourcc('l', 'y', 'i', 'd');
        constexpr std::uint32_t kNameSourceKey = Fourcc('l', 'n', 's', 'r');
        constexpr std::uint32_t kProtectionKey = Fourcc('l', 's', 'p', 'f');
        constexpr std::uint32_t kBackgroundSource = Fourcc('b', 'g', 'n', 'd');
        constexpr std::uint32_t kLayerSource = Fourcc('l', 'a', 'y', 'r');

        // Bit 0 of the flags; Photoshop sets it on a background and nowhere else by default.
        constexpr std::uint8_t kTransparencyLocked = 0x01;

        // Transparency, position and bit 3, which the specification leaves out, as Photoshop writes it.
        constexpr std::uint32_t kBackgroundProtection = 0x0000000D;
        constexpr std::int16_t kTransparencyId = -1;
        constexpr std::uint16_t kMaxChannels = 56;

        // Bit 3, written by Photoshop 5.0 and later: Photoshop sets it on every layer.
        constexpr std::uint8_t kFlagsPhotoshop5 = 0x08;

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

        // A length-prefixed field of the extra data, which must not run past its end.
        std::vector<std::uint8_t> ReadExtraField(BigEndianReader& reader, std::size_t end, const char* what)
        {
            const std::uint32_t length = reader.ReadU32();
            if (reader.Tell() > end || length > end - reader.Tell())
                throw std::runtime_error(
                    std::string("ffpsd: layer ") + what + " claims " + std::to_string(length) +
                    " bytes, more than the extra data holds");

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

        void SetU32Block(LayerRecord& record, std::uint32_t key, std::uint32_t value)
        {
            BigEndianWriter writer(sizeof(std::uint32_t));
            writer.WriteU32(value);
            FindOrAppendTaggedBlock(record.blocks, key).data = writer.Take();
        }

        std::unique_ptr<TaggedBlock> UnicodeName(const std::string& name)
        {
            BigEndianWriter writer;
            WriteUnicodeString(writer, name);

            auto block = std::make_unique<TaggedBlock>();
            block->signature = kBlockSignature;
            block->key = kUnicodeNameKey;
            block->data = writer.Take();
            return block;
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
                "ffpsd: layer at offset " + std::to_string(record_start) + " claims " + std::to_string(channel_count) +
                " channels");

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
                "ffpsd: expected 8BIM before the blend mode of the layer at offset " + std::to_string(record_start) +
                ", got '" + FourccString(record.blend_signature) + "'");

        record.blend_key = reader.ReadU32();
        record.opacity = reader.ReadU8();
        record.clipping = reader.ReadU8();
        record.flags = reader.ReadU8();
        reader.Skip(1); // filler

        const std::uint32_t extra_length = reader.ReadU32();
        if (extra_length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: layer extra data claims " + std::to_string(extra_length) + " bytes, only " +
                std::to_string(reader.GetRemaining()) + " left");
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

    void WriteLayerRecord(
        BigEndianWriter& writer, const LayerRecord& record,
        const std::vector<const std::vector<std::uint8_t>*>& channels, bool is_psb)
    {
        writer.WriteI32(record.bounds.top);
        writer.WriteI32(record.bounds.left);
        writer.WriteI32(record.bounds.bottom);
        writer.WriteI32(record.bounds.right);

        writer.WriteU16(static_cast<std::uint16_t>(record.channels.size()));
        for (std::size_t i = 0; i < record.channels.size(); ++i)
        {
            const std::size_t size = channels.at(i)->size();
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
        WriteTaggedBlocks(writer, record.blocks, is_psb);
        writer.PatchLength(extra_length, false);
    }

    std::uint32_t GetLayerId(const LayerRecord& record) noexcept
    {
        const TaggedBlock* block = GetTaggedBlockByKey(record.blocks, kLayerIdKey);
        if (block == nullptr || block->data.size() < sizeof(std::uint32_t))
            return 0;

        BigEndianReader reader(block->data);
        return reader.ReadU32();
    }

    void SetLayerId(LayerRecord& record, std::uint32_t id)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(id);
        FindOrAppendTaggedBlock(record.blocks, kLayerIdKey).data = writer.Take();
    }

    bool IsBackground(const LayerRecord& record) noexcept
    {
        const TaggedBlock* block = GetTaggedBlockByKey(record.blocks, kNameSourceKey);
        if (block == nullptr || block->data.size() < sizeof(std::uint32_t))
            return false;

        BigEndianReader reader(block->data);
        return reader.ReadU32() == kBackgroundSource;
    }

    void MarkAsBackground(LayerRecord& record)
    {
        record.flags = static_cast<std::uint8_t>(record.flags | kTransparencyLocked);
        SetU32Block(record, kNameSourceKey, kBackgroundSource);
        SetU32Block(record, kProtectionKey, kBackgroundProtection);
    }

    void UnmarkBackground(LayerRecord& record)
    {
        record.flags = static_cast<std::uint8_t>(record.flags & ~kTransparencyLocked);
        SetU32Block(record, kNameSourceKey, kLayerSource);
        SetU32Block(record, kProtectionKey, 0);
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
        copy.blocks = CloneTaggedBlocks(source.blocks);
        return copy;
    }

    SamplesView ViewOf(const Image& image) noexcept
    {
        SamplesView view;
        view.data = image.bytes.data();
        view.size = image.bytes.size();
        view.width = image.width;
        view.height = image.height;
        view.channel_count = image.channel_count;
        view.depth = image.depth;
        return view;
    }

    bool HasLayerMask(const LayerRecord& record) noexcept
    {
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                return true;
        }
        return false;
    }

    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width > max_side || height > max_side)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " layer exceeds " +
                std::to_string(max_side) + " pixels a side");
    }

    std::vector<ChannelImageData> EncodeLayerChannels(const SamplesView& samples, std::size_t color_count, bool is_psb)
    {
        std::vector<ChannelImageData> channels;
        if (samples.width == 0 || samples.height == 0 || samples.channel_count == 0)
        {
            // Photoshop gives an empty layer every channel, each just a compression field.
            channels.push_back(EncodeChannel(kTransparencyId, nullptr, 0, 0, 1, is_psb));
            for (std::size_t i = 0; i < color_count; ++i)
                channels.push_back(EncodeChannel(static_cast<std::int16_t>(i), nullptr, 0, 0, 1, is_psb));
            return channels;
        }

        if (samples.depth != 8 && samples.depth != 16 && samples.depth != 32)
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(samples.depth));
        if (samples.channel_count != color_count && samples.channel_count != color_count + 1)
            throw std::invalid_argument(
                "ffpsd: the document needs " + std::to_string(color_count) + " color channels, the image has " +
                std::to_string(samples.channel_count));

        const std::size_t plane_samples = std::size_t{samples.width} * samples.height;
        const std::size_t sample_size = samples.depth / 8u;
        const std::size_t needed = plane_samples * samples.channel_count * sample_size;
        if (samples.data == nullptr || samples.size != needed)
            throw std::invalid_argument(
                "ffpsd: samples hold " + std::to_string(samples.size) + " bytes, the geometry needs " +
                std::to_string(needed));

        const auto plane = [&](std::size_t index) { return samples.data + index * plane_samples * sample_size; };

        // Transparency is declared first, as Photoshop writes it.
        if (samples.channel_count > color_count)
            channels.push_back(
                EncodeChannel(kTransparencyId, plane(color_count), samples.width, samples.height, sample_size, is_psb));
        for (std::size_t i = 0; i < color_count; ++i)
            channels.push_back(EncodeChannel(
                static_cast<std::int16_t>(i), plane(i), samples.width, samples.height, sample_size, is_psb));
        return channels;
    }

    Rect PlaceSamples(const SamplesView& samples, std::int32_t top, std::int32_t left)
    {
        if (samples.width == 0 || samples.height == 0 || samples.channel_count == 0)
            return {top, left, top, left};

        const std::int64_t bottom = std::int64_t{top} + samples.height;
        const std::int64_t right = std::int64_t{left} + samples.width;
        if (bottom > std::numeric_limits<std::int32_t>::max() || right > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
        return {top, left, static_cast<std::int32_t>(bottom), static_cast<std::int32_t>(right)};
    }

    Image DecodeLayerPixels(const LayerRecord& record, std::size_t color_count, std::uint16_t depth, bool is_psb)
    {
        if (depth != 8 && depth != 16 && depth != 32)
            throw std::runtime_error("ffpsd: " + std::to_string(depth) + " bit layer pixels are not supported");

        Image image;
        image.depth = depth;
        const std::int64_t width = std::int64_t{record.bounds.right} - record.bounds.left;
        const std::int64_t height = std::int64_t{record.bounds.bottom} - record.bounds.top;
        if (width <= 0 || height <= 0)
            return image;

        const ChannelImageData* transparency = nullptr;
        std::vector<const ChannelImageData*> colors(color_count, nullptr);
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id == kTransparencyId)
                transparency = &channel;
            else if (channel.id >= 0 && static_cast<std::size_t>(channel.id) < color_count)
                colors[static_cast<std::size_t>(channel.id)] = &channel;
        }
        for (std::size_t i = 0; i < color_count; ++i)
        {
            if (colors[i] == nullptr)
                throw std::runtime_error("ffpsd: the layer has no channel " + std::to_string(i));
        }
        if (transparency != nullptr)
            colors.push_back(transparency);

        image.width = static_cast<std::uint32_t>(width);
        image.height = static_cast<std::uint32_t>(height);
        image.channel_count = static_cast<std::uint16_t>(colors.size());
        image.bytes.resize(image.GetSizeBytes());

        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t plane = row_bytes * image.height;
        for (std::size_t i = 0; i < colors.size(); ++i)
            DecodePixelData(
                colors[i]->raw.data(), colors[i]->raw.size(), image.height, row_bytes, is_psb,
                image.bytes.data() + i * plane);

        SwapSampleBytes(image.bytes, image.GetBytesPerSample());
        return image;
    }

    void ReplaceLayerPixels(LayerRecord& record, const SamplesView& samples, std::size_t color_count, bool is_psb)
    {
        std::vector<ChannelImageData> channels = EncodeLayerChannels(samples, color_count, is_psb);
        const Rect bounds = PlaceSamples(samples, record.bounds.top, record.bounds.left);

        // Masks have their own rectangle, so new pixels leave them as they were.
        for (ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                channels.push_back(std::move(channel));
        }
        record.channels = std::move(channels);
        record.bounds = bounds;
    }

    LayerRecord CreateLayerRecord(
        const std::string& name, const SamplesView& samples, std::int32_t top, std::int32_t left,
        std::size_t color_count, bool is_psb)
    {
        LayerRecord record;
        record.name = name;
        record.channels = EncodeLayerChannels(samples, color_count, is_psb);
        record.bounds = PlaceSamples(samples, top, left);
        record.flags = kFlagsPhotoshop5;
        record.blending_ranges = DefaultBlendingRanges();
        record.blocks.push_back(UnicodeName(name));
        return record;
    }
} // namespace ffpsd::detail
