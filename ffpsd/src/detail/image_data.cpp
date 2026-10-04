#include "detail/image_data.hpp"

#include "detail/image.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/compression.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    PixelData
    ParseImageData(BigEndianReader& reader, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth)
    {
        std::vector<std::uint8_t> bytes(reader.GetRemaining());
        if (!bytes.empty())
            reader.ReadU8Array(bytes.data(), bytes.size());
        return PixelData(std::move(bytes), std::size_t{height} * channel_count, RowBytes(width, depth), SampleBytes(depth));
    }

    Image DecodeImageData(
        const PixelData& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        ColorMode color_mode, bool is_psb)
    {
        if (data.IsEmpty())
            return Image();
        if (depth != 8 && depth != 16 && depth != 32)
            throw std::runtime_error("ffpsd: a " + std::to_string(depth) + " bit merged image is not supported");

        Image image;
        image.width = width;
        image.height = height;
        image.channel_count = channel_count;
        image.depth = depth;
        image.color_mode = color_mode;
        image.bytes.resize(image.GetSizeBytes());
        data.Decode(is_psb, image.bytes.data());
        return image;
    }

    PixelData EncodeImageData(const ImageView& image, bool is_psb)
    {
        CheckImage(image);

        const std::size_t rows = std::size_t{image.height} * image.channel_count;
        return PixelData::Encode(
            image.data, rows, RowBytes(image.width, image.depth), image.GetBytesPerSample(), is_psb, Compression::kRleOrRaw);
    }

    PixelData EncodeBlankImageData(
        std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, bool is_psb, Compression compression)
    {
        const std::size_t row_bytes = RowBytes(width, depth);
        const std::size_t sample_size = SampleBytes(depth);
        const std::size_t rows = std::size_t{height} * channel_count;
        if (compression == Compression::kRaw)
            return PixelData(std::vector<std::uint8_t>(sizeof(std::uint16_t) + rows * row_bytes, 0), rows, row_bytes, sample_size);

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
        return PixelData(writer.Take(), rows, row_bytes, sample_size);
    }
} // namespace ffpsd::detail
