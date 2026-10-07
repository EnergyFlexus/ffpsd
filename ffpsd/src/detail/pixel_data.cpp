#include "detail/pixel_data.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/byte_order.hpp"
#include "detail/io/compression.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kSampleStep = 16;

        template <typename T> void SwapSamples(std::uint8_t* bytes, std::size_t size) noexcept
        {
            for (std::size_t at = 0; at + sizeof(T) <= size; at += sizeof(T))
            {
                T value;
                std::memcpy(&value, bytes + at, sizeof(T));
                value = Swapped(value);
                std::memcpy(bytes + at, &value, sizeof(T));
            }
        }

        // Big endian to native and back: the same swap.
        void SwapSampleBytes(std::uint8_t* bytes, std::size_t size, std::size_t sample_size) noexcept
        {
            if (sample_size == 2)
                SwapSamples<std::uint16_t>(bytes, size);
            else if (sample_size == 4)
                SwapSamples<std::uint32_t>(bytes, size);
        }

        bool IsRawOrRle(std::uint16_t compression) noexcept
        {
            return compression == kCompressionRaw || compression == kCompressionRle;
        }

        // Stored rows in order, each checked against the data: raw ones where they lie, RLE ones unpacked into the caller's scratch.
        class RowReader
        {
        public:
            struct Rows
            {
                const std::uint8_t* bytes;
                std::size_t count;
            };

            RowReader(const std::vector<std::uint8_t>& bytes, std::size_t rows, std::size_t row_bytes, bool is_psb)
                : bytes_(bytes)
                , reader_(bytes)
                , rows_(rows)
                , row_bytes_(row_bytes)
            {
                compression_ = reader_.ReadU16();
                if (compression_ == kCompressionRaw)
                {
                    if (reader_.GetRemaining() < rows * row_bytes)
                        throw std::runtime_error(
                            "ffpsd: raw pixel data holds " + std::to_string(reader_.GetRemaining()) + " bytes, needs " +
                            std::to_string(rows * row_bytes));
                }
                else if (compression_ == kCompressionRle)
                {
                    // One byte count per row, all of them before any row.
                    counts_.resize(rows);
                    for (std::size_t& count : counts_)
                        count = is_psb ? reader_.ReadU32() : reader_.ReadU16();
                }
                else
                {
                    throw std::runtime_error("ffpsd: pixel compression " + std::to_string(compression_) + " is not supported yet");
                }
            }

            // All raw rows left at once, where they lie, as one big copy beats many; or one RLE row, unpacked in scratch.
            Rows ReadRows(std::uint8_t* scratch)
            {
                const std::uint8_t* at = bytes_.data() + reader_.Tell();
                if (compression_ == kCompressionRaw)
                {
                    const std::size_t count = rows_ - row_;
                    reader_.Skip(count * row_bytes_);
                    row_ = rows_;
                    return {at, count};
                }

                const std::size_t count = counts_[row_];
                if (count > reader_.GetRemaining())
                    throw std::runtime_error(
                        "ffpsd: pixel row " + std::to_string(row_) + " claims " + std::to_string(count) + " bytes, only " +
                        std::to_string(reader_.GetRemaining()) + " left");
                UnpackBits(at, count, scratch, row_bytes_);
                reader_.Skip(count);
                ++row_;
                return {scratch, 1};
            }

        private:
            const std::vector<std::uint8_t>& bytes_;
            BigEndianReader reader_;
            std::size_t rows_ = 0;
            std::size_t row_bytes_ = 0;
            std::uint16_t compression_ = 0;
            std::vector<std::size_t> counts_;
            std::size_t row_ = 0;
        };

        // The same packed rows behind counts of the other width; empty when a PSB count does not fit a PSD.
        std::optional<std::vector<std::uint8_t>>
        RewriteRowCounts(const std::vector<std::uint8_t>& bytes, std::size_t rows, bool from_psb, bool to_psb)
        {
            BigEndianReader reader(bytes);
            reader.Skip(sizeof(std::uint16_t));
            std::vector<std::uint32_t> counts(rows);
            for (std::uint32_t& count : counts)
                count = from_psb ? reader.ReadU32() : reader.ReadU16();

            const auto too_big = [](std::uint32_t count) { return count > std::numeric_limits<std::uint16_t>::max(); };
            if (!to_psb && std::any_of(counts.begin(), counts.end(), too_big))
                return std::nullopt;

            BigEndianWriter writer(bytes.size() + rows * sizeof(std::uint16_t));
            writer.WriteU16(kCompressionRle);
            for (const std::uint32_t count : counts)
            {
                if (to_psb)
                    writer.WriteU32(count);
                else
                    writer.WriteU16(static_cast<std::uint16_t>(count));
            }
            writer.WriteU8Array(bytes.data() + reader.Tell(), reader.GetRemaining());
            return writer.Take();
        }
    } // namespace

    PixelData::PixelData(std::vector<std::uint8_t> bytes, std::size_t rows, std::size_t row_bytes, std::size_t sample_size)
        : bytes_(std::move(bytes))
        , rows_(rows)
        , row_bytes_(row_bytes)
        , sample_size_(sample_size)
    {
    }

    std::optional<PixelData> PixelData::Pack(
        const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, std::size_t sample_size, bool is_psb, bool smallest, bool& fits)
    {
        const std::size_t total = rows * row_bytes;
        const std::size_t count_size = is_psb ? sizeof(std::uint32_t) : sizeof(std::uint16_t);
        const std::size_t count_max = is_psb ? std::numeric_limits<std::uint32_t>::max() : std::numeric_limits<std::uint16_t>::max();
        const std::size_t header = sizeof(std::uint16_t);
        const std::size_t table = rows * count_size;

        fits = true;
        if (total == 0)
            return std::nullopt;

        // One row at a time goes big endian, so the samples are never copied whole.
        std::vector<std::uint8_t> swapped(sample_size > 1 ? row_bytes : 0);
        const auto pack_row = [&](std::size_t row, std::vector<std::uint8_t>& to) {
            const std::uint8_t* samples = data + row * row_bytes;
            if (!swapped.empty())
            {
                std::memcpy(swapped.data(), samples, row_bytes);
                SwapSampleBytes(swapped.data(), row_bytes, sample_size);
                samples = swapped.data();
            }
            PackBits(samples, row_bytes, to);
        };

        // Every 16th row across the channel, so noise at its top or bottom alone does not decide: no smaller there, noise mostly.
        if (smallest)
        {
            std::vector<std::uint8_t> sample;
            std::size_t sampled = 0;
            for (std::size_t row = 0; row < rows; row += kSampleStep, ++sampled)
                pack_row(row, sample);
            if (sample.size() >= sampled * row_bytes)
                return std::nullopt;
        }

        // Room for the row counts, then the packed rows straight after.
        std::vector<std::uint8_t> out(header + table, 0);
        out[1] = static_cast<std::uint8_t>(kCompressionRle);
        for (std::size_t row = 0; row < rows; ++row)
        {
            const std::size_t start = out.size();
            pack_row(row, out);
            const std::size_t count = out.size() - start;

            std::uint8_t* field = out.data() + header + row * count_size;
            for (std::size_t i = 0; i < count_size; ++i)
                field[i] = static_cast<std::uint8_t>(count >> (8 * (count_size - 1 - i)));

            // Stops as soon as RLE cannot win or a count does not fit its field.
            fits = count <= count_max;
            if (!fits || (smallest && out.size() >= header + total))
                return std::nullopt;
        }
        return PixelData(std::move(out), rows, row_bytes, sample_size);
    }

    PixelData PixelData::Encode(
        const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, std::size_t sample_size, bool is_psb, Compression compression)
    {
        const bool smallest = compression == Compression::kRleOrRaw;
        bool fits = true;
        if (compression != Compression::kRaw)
        {
            if (std::optional<PixelData> packed = Pack(data, rows, row_bytes, sample_size, is_psb, smallest, fits))
                return std::move(*packed);
        }

        const std::size_t total = rows * row_bytes;
        std::vector<std::uint8_t> out(sizeof(std::uint16_t), 0);
        out.insert(out.end(), data, data + total);
        SwapSampleBytes(out.data() + sizeof(std::uint16_t), total, sample_size);
        PixelData raw(std::move(out), rows, row_bytes, sample_size);
        raw.rle_loses_ = smallest && total != 0 && fits;
        return raw;
    }

    const std::vector<std::uint8_t>& PixelData::GetBytes() const noexcept
    {
        return bytes_;
    }

    bool PixelData::IsEmpty() const noexcept
    {
        return bytes_.empty();
    }

    std::uint16_t PixelData::GetCompression() const noexcept
    {
        if (bytes_.size() < sizeof(std::uint16_t))
            return kCompressionRaw;
        return static_cast<std::uint16_t>(bytes_[0] << 8 | bytes_[1]);
    }

    void PixelData::Decode(bool is_psb, std::uint8_t* out) const
    {
        RowReader rows(bytes_, rows_, row_bytes_, is_psb);
        for (std::size_t row = 0; row < rows_;)
        {
            std::uint8_t* target = out + row * row_bytes_;
            const RowReader::Rows got = rows.ReadRows(target);
            if (got.bytes != target)
                std::memcpy(target, got.bytes, got.count * row_bytes_);
            row += got.count;
        }
        SwapSampleBytes(out, rows_ * row_bytes_, sample_size_);
    }

    void PixelData::Decode(bool is_psb, std::vector<std::uint8_t>& out) const
    {
        // A vector takes rows only appended: packed ones unpack in a line that stays in cache, as an insert per run costs more.
        RowReader rows(bytes_, rows_, row_bytes_, is_psb);
        const std::size_t start = out.size();
        std::vector<std::uint8_t> line(row_bytes_);
        for (std::size_t row = 0; row < rows_;)
        {
            const RowReader::Rows got = rows.ReadRows(line.data());
            out.insert(out.end(), got.bytes, got.bytes + got.count * row_bytes_);
            row += got.count;
        }
        SwapSampleBytes(out.data() + start, rows_ * row_bytes_, sample_size_);
    }

    bool PixelData::NeedsConversion(bool from_psb, bool to_psb, Compression compression) const noexcept
    {
        // Just a compression field: no rows, the same in both formats.
        if (bytes_.size() <= sizeof(std::uint16_t))
            return false;

        const std::uint16_t current = GetCompression();
        if (current == kCompressionRle && from_psb != to_psb)
            return true;
        if (!IsRawOrRle(current) || rows_ * row_bytes_ == 0)
            return false;

        switch (compression)
        {
        case Compression::kRaw:
            return current != kCompressionRaw;
        case Compression::kRle:
            return current != kCompressionRle;
        case Compression::kRleOrRaw:
            // RLE stays as it is; raw is tried once, unless RLE has already lost on these very bytes.
            return current == kCompressionRaw && !rle_loses_;
        default:
            return false;
        }
    }

    std::optional<PixelData> PixelData::Converted(bool from_psb, bool to_psb, Compression compression) const
    {
        if (!NeedsConversion(from_psb, to_psb, compression))
            return std::nullopt;

        if (GetCompression() == kCompressionRle && compression != Compression::kRaw)
        {
            if (std::optional<std::vector<std::uint8_t>> bytes = RewriteRowCounts(bytes_, rows_, from_psb, to_psb))
                return PixelData(std::move(*bytes), rows_, row_bytes_, sample_size_);
        }

        // Raw bytes are big endian as the file wants them, so they pack as they are; short ones go to Decode, which reports them.
        if (GetCompression() == kCompressionRaw && bytes_.size() >= sizeof(std::uint16_t) + rows_ * row_bytes_)
        {
            bool fits = true;
            std::optional<PixelData> packed =
                Pack(bytes_.data() + sizeof(std::uint16_t), rows_, row_bytes_, 1, to_psb, compression == Compression::kRleOrRaw, fits);
            if (packed.has_value())
                packed->sample_size_ = sample_size_;
            return packed;
        }

        std::vector<std::uint8_t> samples;
        samples.reserve(rows_ * row_bytes_);
        Decode(from_psb, samples);
        return Encode(samples.data(), rows_, row_bytes_, sample_size_, to_psb, compression);
    }
} // namespace ffpsd::detail
