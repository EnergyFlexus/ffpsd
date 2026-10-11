#ifndef FFPSD_DETAIL_PIXEL_DATA_HPP_
#define FFPSD_DETAIL_PIXEL_DATA_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // The compression field; ZIP, 2 and 3, is kept as it is.
    constexpr std::uint16_t kCompressionRaw = 0;
    constexpr std::uint16_t kCompressionRle = 1;

    // A layer channel or section 5 as stored, big endian; RLE row counts are 2 bytes in a PSD and 4 in a PSB.
    class PixelData
    {
    public:
        PixelData() = default;

        // Stored bytes, the rows they hold and the size of a sample in them.
        PixelData(std::vector<std::uint8_t> bytes, std::size_t rows, std::size_t row_bytes, std::size_t sample_size);

        // Native rows; a row whose count does not fit its field leaves the channel raw.
        static PixelData Encode(
            const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, std::size_t sample_size, bool is_psb,
            Compression compression);

        // A planar image, every plane's rows one after another, RLE when smaller.
        static PixelData Encode(const ImageView& image, bool is_psb);

        // Zeros, for a file without a composite; in RLE a large document costs a few bytes a row.
        static PixelData Blank(
            std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth, bool is_psb,
            Compression compression);

        const std::vector<std::uint8_t>& GetBytes() const noexcept;
        bool IsEmpty() const noexcept;

        // The compression field, or raw for data too short to have one.
        std::uint16_t GetCompression() const noexcept;

        // Raw and RLE, rows * row_bytes bytes in native byte order.
        void Decode(bool is_psb, std::uint8_t* out) const;

        // The pixels in the shape info gives, which matches the rows; empty data leaves them as they are.
        Image Decode(const ImageInfo& info, bool is_psb) const;

        // The same into the caller's memory, exactly info's GetSizeBytes of it.
        void Decode(const ImageInfo& info, bool is_psb, std::uint8_t* out, std::size_t size) const;

        // None while the stored bytes stay, raw RLE could not shrink included; RLE changing format rewrites only the counts.
        std::optional<PixelData> Converted(bool from_psb, bool to_psb, Compression compression) const;

    private:
        // RLE rows; none where a count does not fit its field or, when smallest, RLE comes out no smaller.
        static std::optional<PixelData> Pack(
            const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, std::size_t sample_size, bool is_psb, bool smallest,
            bool& fits);

        bool NeedsConversion(bool from_psb, bool to_psb, Compression compression) const noexcept;

        std::vector<std::uint8_t> bytes_;
        std::size_t rows_ = 0;
        std::size_t row_bytes_ = 0;
        std::size_t sample_size_ = 1;

        // Raw because RLE came out no smaller, so an RLE save would only pack it again for nothing.
        bool rle_loses_ = false;
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_PIXEL_DATA_HPP_
