#include "detail/layer_and_mask/channel_image_data.hpp"

#include "detail/samples.hpp"

#include <algorithm>
#include <ffpsd/bytes.hpp>
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
        std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<std::size_t>(length));

        ChannelImageData channel;
        channel.id = id;
        const auto rows = static_cast<std::size_t>(std::max<std::int64_t>(bounds.GetHeight(), 0));
        const auto width = static_cast<std::uint32_t>(std::max<std::int64_t>(bounds.GetWidth(), 0));
        channel.data = PixelData(std::move(bytes), rows, RowBytes(width, depth), SampleBytes(depth));
        return channel;
    }

    ChannelImageData ChannelImageData::Opaque(std::int16_t id, std::uint32_t width, std::uint32_t height, std::uint16_t depth, bool is_psb)
    {
        const std::size_t count = std::size_t{width} * height;
        Bytes samples(count * SampleBytes(depth));
        FillFull(samples.data(), count, depth);
        return Encode(id, samples.data(), width, height, depth, is_psb);
    }

    ChannelImageData ChannelImageData::Encode(
        std::int16_t id, const std::uint8_t* samples, std::uint32_t width, std::uint32_t height, std::uint16_t depth, bool is_psb)
    {
        if (width == 0 || height == 0)
            return Empty(id);

        ChannelImageData channel;
        channel.id = id;
        channel.data = PixelData::Encode(samples, height, RowBytes(width, depth), SampleBytes(depth), is_psb, Compression::kRleOrRaw);
        return channel;
    }

    ChannelImageData ChannelImageData::Empty(std::int16_t id)
    {
        ChannelImageData channel;
        channel.id = id;
        channel.data = PixelData(std::vector<std::uint8_t>(sizeof(std::uint16_t), 0), 0, 0, 1);
        return channel;
    }
} // namespace ffpsd::detail
