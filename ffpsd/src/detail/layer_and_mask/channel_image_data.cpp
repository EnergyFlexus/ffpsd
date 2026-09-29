#include "detail/layer_and_mask/channel_image_data.hpp"

#include "detail/pixel_data.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    ChannelImageData ParseChannelImageData(BigEndianReader& reader, std::size_t end, std::int16_t id, std::uint64_t length)
    {
        if (length > end - reader.Tell())
            throw std::runtime_error(
                "ffpsd: channel " + std::to_string(id) + " claims " + std::to_string(length) + " bytes, only " +
                std::to_string(end - reader.Tell()) + " left in the layer info");

        ChannelImageData channel;
        channel.id = id;
        channel.raw.resize(static_cast<std::size_t>(length));
        if (!channel.raw.empty())
            reader.ReadU8Array(channel.raw.data(), channel.raw.size());
        return channel;
    }

    ChannelImageData EncodeChannel(
        std::int16_t id, const std::uint8_t* samples, std::size_t width, std::size_t height, std::size_t bytes_per_sample, bool is_psb)
    {
        ChannelImageData channel;
        channel.id = id;

        const std::size_t size = width * height * bytes_per_sample;
        if (size == 0)
        {
            channel.raw.assign(sizeof(std::uint16_t), 0);
            return channel;
        }

        std::vector<std::uint8_t> big_endian(samples, samples + size);
        SwapSampleBytes(big_endian, bytes_per_sample);
        channel.raw = EncodePixelData(big_endian.data(), height, width * bytes_per_sample, is_psb, kCompressionRle);
        return channel;
    }
} // namespace ffpsd::detail
