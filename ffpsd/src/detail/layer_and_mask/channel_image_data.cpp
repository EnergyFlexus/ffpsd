#include "detail/layer_and_mask/channel_image_data.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    ChannelImageData ParseChannelImageData(
        BigEndianReader& reader, std::size_t end, std::int16_t id, std::uint64_t length, const Rect& bounds, std::uint16_t depth)
    {
        if (length > end - reader.Tell())
            throw std::runtime_error(
                "ffpsd: channel " + std::to_string(id) + " claims " + std::to_string(length) + " bytes, only " +
                std::to_string(end - reader.Tell()) + " left in the layer info");

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (!bytes.empty())
            reader.ReadU8Array(bytes.data(), bytes.size());

        ChannelImageData channel;
        channel.id = id;
        if (id < kTransparencyId)
        {
            channel.data = PixelData::Unsized(std::move(bytes));
            return channel;
        }

        const auto rows = static_cast<std::size_t>(std::max(bounds.GetHeight(), 0));
        const auto width = static_cast<std::uint32_t>(std::max(bounds.GetWidth(), 0));
        channel.data = PixelData(std::move(bytes), rows, RowBytes(width, depth));
        return channel;
    }

    ChannelImageData EncodeChannelImageData(
        std::int16_t id, const std::uint8_t* samples, std::size_t width, std::size_t height, std::size_t bytes_per_sample, bool is_psb)
    {
        ChannelImageData channel;
        channel.id = id;

        const std::size_t size = width * height * bytes_per_sample;
        if (size == 0)
        {
            channel.data = PixelData(std::vector<std::uint8_t>(sizeof(std::uint16_t), 0), 0, 0);
            return channel;
        }

        std::vector<std::uint8_t> big_endian(samples, samples + size);
        SwapSampleBytes(big_endian, bytes_per_sample);
        channel.data = PixelData::Encode(big_endian.data(), height, width * bytes_per_sample, is_psb, kCompressionRle);
        return channel;
    }
} // namespace ffpsd::detail
