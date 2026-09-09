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

    // A layer channel or section 5: compression, then raw rows or RLE counts and rows; out stays big endian.
    void DecodePixelData(
        const std::uint8_t* data, std::size_t size, std::size_t rows, std::size_t row_bytes, bool is_psb,
        std::uint8_t* out);

    // RLE falls back to raw when it is not smaller or a row's count does not fit its field.
    std::vector<std::uint8_t> EncodePixelData(
        const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, bool is_psb, std::uint16_t compression);

    // The compression field, or raw for data too short to have one.
    std::uint16_t GetPixelCompression(const std::vector<std::uint8_t>& data) noexcept;

    // RLE row counts are 2 bytes in a PSD and 4 in a PSB, so such data is tied to its format.
    bool IsRlePixelData(const std::vector<std::uint8_t>& data) noexcept;

    // The same rows, unpacked and written again.
    std::vector<std::uint8_t> ConvertPixelData(
        const std::vector<std::uint8_t>& data, std::size_t rows, std::size_t row_bytes, bool from_psb, bool to_psb,
        std::uint16_t compression);

    // The file keeps samples big endian, Image in native order; the swap goes both ways.
    void SwapSampleBytes(std::vector<std::uint8_t>& bytes, std::size_t sample_size) noexcept;
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_PIXEL_DATA_HPP_
