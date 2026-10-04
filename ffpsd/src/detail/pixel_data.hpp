#ifndef FFPSD_DETAIL_PIXEL_DATA_HPP_
#define FFPSD_DETAIL_PIXEL_DATA_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    // The compression field; ZIP, 2 and 3, is kept as it is.
    constexpr std::uint16_t kCompressionRaw = 0;
    constexpr std::uint16_t kCompressionRle = 1;

    // A bitmap's bits count as one byte samples, which need no swap.
    constexpr std::size_t SampleBytes(std::uint16_t depth) noexcept
    {
        return depth <= 8 ? 1 : depth / 8u;
    }

    // Bitmap rows are padded to a whole byte.
    constexpr std::size_t RowBytes(std::uint32_t width, std::uint16_t depth) noexcept
    {
        return depth == 1 ? (std::size_t{width} + 7) / 8 : std::size_t{width} * SampleBytes(depth);
    }

    // A layer channel or section 5 as stored, big endian; RLE row counts are 2 bytes in a PSD and 4 in a PSB.
    class PixelData
    {
    public:
        PixelData() = default;

        // Stored bytes, the rows they hold and the size of a sample in them.
        PixelData(std::vector<std::uint8_t> bytes, std::size_t rows, std::size_t row_bytes, std::size_t sample_size);

        // Native rows; RLE falls back to raw when it is not smaller or a row's count does not fit its field.
        static PixelData Encode(
            const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, std::size_t sample_size, bool is_psb,
            std::uint16_t compression);

        const std::vector<std::uint8_t>& GetBytes() const noexcept;
        bool IsEmpty() const noexcept;

        // The compression field, or raw for data too short to have one.
        std::uint16_t GetCompression() const noexcept;

        // Raw and RLE; out gets rows * row_bytes bytes in native byte order.
        void Decode(bool is_psb, std::uint8_t* out) const;

        // Whether Converted gives other bytes: RLE changing format, or rows packed another way.
        bool NeedsConversion(bool from_psb, bool to_psb, std::uint16_t compression) const noexcept;

        // RLE changing format rewrites only the counts.
        PixelData Converted(bool from_psb, bool to_psb, std::uint16_t compression) const;

    private:
        std::vector<std::uint8_t> bytes_;
        std::size_t rows_ = 0;
        std::size_t row_bytes_ = 0;
        std::size_t sample_size_ = 1;
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_PIXEL_DATA_HPP_
