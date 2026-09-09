#include "detail/image_data.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/compression.hpp"
#include "detail/pixel_data.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
    } // namespace

    std::vector<std::uint8_t> ParseImageData(BigEndianReader& reader)
    {
        std::vector<std::uint8_t> data(reader.GetRemaining());
        if (!data.empty())
            reader.ReadU8Array(data.data(), data.size());
        return data;
    }

    Image DecodeImageData(
        const std::vector<std::uint8_t>& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count,
        std::uint16_t depth, bool is_psb)
    {
        if (data.empty())
            return Image();
        if (depth != 8 && depth != 16 && depth != 32)
            throw std::runtime_error("ffpsd: a " + std::to_string(depth) + " bit merged image is not supported");

        Image image;
        image.width = width;
        image.height = height;
        image.channel_count = channel_count;
        image.depth = depth;
        image.bytes.resize(image.GetSizeBytes());

        // Every channel's rows in one run, as if the whole section were one tall channel.
        const std::size_t row_bytes = std::size_t{width} * image.GetBytesPerSample();
        const std::size_t rows = std::size_t{height} * channel_count;
        DecodePixelData(data.data(), data.size(), rows, row_bytes, is_psb, image.bytes.data());

        SwapSampleBytes(image.bytes, image.GetBytesPerSample());
        return image;
    }

    std::vector<std::uint8_t> EncodeImageData(const Image& image, bool is_psb)
    {
        if (image.bytes.size() != image.GetSizeBytes())
            throw std::invalid_argument(
                "ffpsd: image holds " + std::to_string(image.bytes.size()) + " bytes, its geometry needs " +
                std::to_string(image.GetSizeBytes()));

        std::vector<std::uint8_t> samples = image.bytes;
        SwapSampleBytes(samples, image.GetBytesPerSample());

        // Every channel's rows in one run.
        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t rows = std::size_t{image.height} * image.channel_count;
        return EncodePixelData(samples.data(), rows, row_bytes, is_psb, kCompressionRle);
    }

    std::vector<std::uint8_t> EncodeBlankImageData(
        std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, bool is_psb,
        std::uint16_t compression)
    {
        const std::size_t row_bytes = depth == 1 ? (std::size_t{width} + 7) / 8 : std::size_t{width} * (depth / 8u);
        const std::size_t rows = std::size_t{height} * channel_count;
        if (compression == kCompressionRaw)
        {
            std::vector<std::uint8_t> raw(sizeof(std::uint16_t) + rows * row_bytes, 0);
            return raw;
        }

        std::vector<std::uint8_t> packed;
        const std::vector<std::uint8_t> zeros(row_bytes, 0);
        PackBits(zeros.data(), zeros.size(), packed);

        BigEndianWriter writer;
        writer.WriteU16(kCompressionRle);
        for (std::size_t i = 0; i < rows; ++i)
        {
            if (is_psb)
                writer.WriteU32(static_cast<std::uint32_t>(packed.size()));
            else
                writer.WriteU16(static_cast<std::uint16_t>(packed.size()));
        }
        for (std::size_t i = 0; i < rows; ++i)
            writer.WriteU8Array(packed.data(), packed.size());
        return writer.Take();
    }
} // namespace ffpsd::detail
