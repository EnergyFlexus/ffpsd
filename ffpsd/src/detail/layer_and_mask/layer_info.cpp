#include "detail/layer_and_mask/layer_info.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kMaxLayers = 32767;
        constexpr std::size_t kAlignment = 4;
    } // namespace

    LayerInfo ParseLayerInfo(BigEndianReader& reader, bool is_psb, std::uint16_t depth)
    {
        const std::uint64_t length = is_psb ? reader.ReadU64() : reader.ReadU32();
        if (length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: layer info claims " + std::to_string(length) + " bytes, only " + std::to_string(reader.GetRemaining()) + " left");

        // The length counts the padding, which is 4 in practice and 2 by the specification.
        return ParseLayerInfoBody(reader, reader.Tell() + static_cast<std::size_t>(length), is_psb, depth);
    }

    LayerInfo ParseLayerInfoBody(BigEndianReader& reader, std::size_t end, bool is_psb, std::uint16_t depth)
    {
        LayerInfo info;
        if (end - reader.Tell() < sizeof(std::int16_t))
        {
            reader.Skip(end - reader.Tell());
            return info;
        }

        const std::int16_t count = reader.ReadI16();
        info.merged_alpha = count < 0;
        const std::size_t layer_count = static_cast<std::size_t>(count < 0 ? -int{count} : int{count});

        std::vector<std::vector<std::uint64_t>> channel_lengths(layer_count);
        info.records.reserve(layer_count);
        for (std::size_t i = 0; i < layer_count; ++i)
        {
            info.records.push_back(ParseLayerRecord(reader, is_psb, channel_lengths[i]));
            if (reader.Tell() > end)
                throw std::runtime_error("ffpsd: layer record " + std::to_string(i) + " runs past the layer info");
        }

        // The blobs follow all the records, in the same order.
        for (std::size_t i = 0; i < layer_count; ++i)
        {
            LayerRecord& record = info.records[i];
            for (std::size_t c = 0; c < record.channels.size(); ++c)
            {
                ChannelImageData& channel = record.channels[c];
                channel = ParseChannelImageData(reader, end, channel.id, channel_lengths[i][c], record.bounds, depth);
            }
        }

        reader.Skip(end - reader.Tell());
        return info;
    }

    void WriteLayerInfo(BigEndianWriter& writer, bool merged_alpha, const std::vector<LayerToWrite>& layers, bool is_psb)
    {
        const std::size_t length = writer.ReserveLength(is_psb);
        if (!layers.empty())
        {
            WriteLayerInfoBody(writer, merged_alpha, layers, is_psb);
            writer.PadFrom(length, kAlignment);
        }
        writer.PatchLength(length, is_psb);
    }

    void WriteLayerInfoBody(BigEndianWriter& writer, bool merged_alpha, const std::vector<LayerToWrite>& layers, bool is_psb)
    {
        if (layers.size() > kMaxLayers)
            throw std::length_error("ffpsd: " + std::to_string(layers.size()) + " layers, the format holds 32767");

        const auto count = static_cast<std::int16_t>(layers.size());
        writer.WriteI16(merged_alpha ? static_cast<std::int16_t>(-count) : count);

        for (const LayerToWrite& layer : layers)
            WriteLayerRecord(writer, *layer.record, layer.channels, is_psb);

        for (const LayerToWrite& layer : layers)
        {
            for (const PixelData* channel : layer.channels)
            {
                const std::vector<std::uint8_t>& bytes = channel->GetBytes();
                if (!bytes.empty())
                    writer.WriteU8Array(bytes.data(), bytes.size());
            }
        }
    }
} // namespace ffpsd::detail
