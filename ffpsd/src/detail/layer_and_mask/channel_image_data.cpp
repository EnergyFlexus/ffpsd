#include "detail/layer_and_mask/channel_image_data.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    ChannelImageData ParseChannelImageData(
        BigEndianReader& reader, std::size_t end, std::int16_t id, std::uint64_t length, const Rect& bounds, std::uint16_t depth)
    {
        if (!reader.FitsLength(length, end))
            throw std::runtime_error(
                "ffpsd: channel " + std::to_string(id) + " claims " + std::to_string(length) + " bytes, more than are left");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (!bytes.empty())
            reader.ReadU8Array(bytes.data(), bytes.size());

        ChannelImageData channel;
        channel.id = id;
        const auto rows = static_cast<std::size_t>(std::max<std::int64_t>(bounds.GetHeight(), 0));
        const auto width = static_cast<std::uint32_t>(std::max<std::int64_t>(bounds.GetWidth(), 0));
        channel.data = PixelData(std::move(bytes), rows, RowBytes(width, depth), SampleBytes(depth));
        return channel;
    }

    ChannelImageData EncodeOpaqueChannel(std::int16_t id, std::size_t width, std::size_t height, std::uint16_t depth, bool is_psb)
    {
        const std::size_t sample_size = SampleBytes(depth);
        std::vector<std::uint8_t> samples(width * height * sample_size, 0xFF);
        if (depth == 32)
        {
            const float full = 1.0f;
            for (std::size_t at = 0; at < samples.size(); at += sizeof(full))
                std::memcpy(samples.data() + at, &full, sizeof(full));
        }
        return EncodeChannelImageData(id, samples.data(), width, height, depth, is_psb);
    }

    ChannelImageData EncodeChannelImageData(
        std::int16_t id, const std::uint8_t* samples, std::size_t width, std::size_t height, std::uint16_t depth, bool is_psb)
    {
        if (width == 0 || height == 0)
            return EmptyChannel(id);

        const std::size_t sample_size = SampleBytes(depth);
        ChannelImageData channel;
        channel.id = id;
        channel.data = PixelData::Encode(samples, height, width * sample_size, sample_size, is_psb, Compression::kRleOrRaw);
        return channel;
    }

    ChannelImageData EmptyChannel(std::int16_t id)
    {
        ChannelImageData channel;
        channel.id = id;
        channel.data = PixelData(std::vector<std::uint8_t>(sizeof(std::uint16_t), 0), 0, 0, 1);
        return channel;
    }
} // namespace ffpsd::detail
