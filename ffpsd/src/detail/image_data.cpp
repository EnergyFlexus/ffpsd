#include "detail/image_data.hpp"

#include "detail/image.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/compression.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    PixelData
    ParseImageData(BigEndianReader& reader, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth)
    {
        std::vector<std::uint8_t> bytes = reader.ReadBytes(reader.GetRemaining());
        return PixelData(std::move(bytes), std::size_t{height} * channel_count, RowBytes(width, depth), SampleBytes(depth));
    }

    ImageInfo ImageDataInfo(
        const PixelData& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        ColorMode color_mode)
    {
        if (data.IsEmpty())
            return ImageInfo();
        if (!IsSampleDepth(depth))
            throw std::runtime_error(UnsupportedDepth(depth));
        return ImageInfo{width, height, channel_count, depth, color_mode};
    }

    Image DecodeImageData(
        const PixelData& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        ColorMode color_mode, bool is_psb)
    {
        const ImageInfo info = ImageDataInfo(data, width, height, channel_count, depth, color_mode);
        if (data.IsEmpty())
            return Image();

        Image image(info);
        data.Decode(is_psb, image.bytes.data());
        return image;
    }

    void DecodeImageData(const PixelData& data, const ImageInfo& info, bool is_psb, std::uint8_t* out, std::size_t size)
    {
        CheckBytesSize(info, out, size);
        if (!data.IsEmpty())
            data.Decode(is_psb, out);
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
