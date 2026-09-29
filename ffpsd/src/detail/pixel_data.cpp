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
        template <typename T> void SwapSamples(std::vector<std::uint8_t>& bytes) noexcept
        {
            for (std::size_t at = 0; at + sizeof(T) <= bytes.size(); at += sizeof(T))
            {
                T value;
                std::memcpy(&value, bytes.data() + at, sizeof(T));
                value = Swapped(value);
                std::memcpy(bytes.data() + at, &value, sizeof(T));
            }
        }

        bool IsRawOrRle(std::uint16_t compression) noexcept
        {
            return compression == kCompressionRaw || compression == kCompressionRle;
        }

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

    PixelData::PixelData(std::vector<std::uint8_t> bytes, std::size_t rows, std::size_t row_bytes)
        : bytes_(std::move(bytes))
        , rows_(rows)
        , row_bytes_(row_bytes)
    {
    }

    PixelData PixelData::Unsized(std::vector<std::uint8_t> bytes)
    {
        PixelData data(std::move(bytes), 0, 0);
        data.sized_ = false;
        return data;
    }

    PixelData PixelData::Encode(const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, bool is_psb, std::uint16_t compression)
    {
        const std::size_t total = rows * row_bytes;
        const std::size_t count_size = is_psb ? sizeof(std::uint32_t) : sizeof(std::uint16_t);
        const std::size_t count_max = is_psb ? std::numeric_limits<std::uint32_t>::max() : std::numeric_limits<std::uint16_t>::max();
        const std::size_t header = sizeof(std::uint16_t);
        const std::size_t table = rows * count_size;

        // Room for the row counts, then the packed rows straight after.
        std::vector<std::uint8_t> out(header + table, 0);
        out[1] = static_cast<std::uint8_t>(kCompressionRle);
        bool packs = compression == kCompressionRle && total != 0;
        for (std::size_t row = 0; row < rows && packs; ++row)
        {
            const std::size_t start = out.size();
            PackBits(data + row * row_bytes, row_bytes, out);
            const std::size_t count = out.size() - start;

            std::uint8_t* field = out.data() + header + row * count_size;
            for (std::size_t i = 0; i < count_size; ++i)
                field[i] = static_cast<std::uint8_t>(count >> (8 * (count_size - 1 - i)));

            // Stops as soon as RLE cannot win or a count does not fit its field.
            packs = count <= count_max && out.size() < header + total;
        }
        if (!packs)
        {
            out.assign(header, 0);
            out.insert(out.end(), data, data + total);
        }
        return PixelData(std::move(out), rows, row_bytes);
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
        BigEndianReader reader(bytes_);
        const std::uint16_t compression = reader.ReadU16();
        const std::size_t total = rows_ * row_bytes_;

        if (compression == kCompressionRaw)
        {
            if (reader.GetRemaining() < total)
                throw std::runtime_error(
                    "ffpsd: raw pixel data holds " + std::to_string(reader.GetRemaining()) + " bytes, needs " + std::to_string(total));
            if (total != 0)
                reader.ReadU8Array(out, total);
            return;
        }

        if (compression != kCompressionRle)
            throw std::runtime_error("ffpsd: pixel compression " + std::to_string(compression) + " is not supported yet");

        // One byte count per row, all of them before any row.
        std::vector<std::size_t> counts(rows_);
        for (std::size_t& count : counts)
            count = is_psb ? reader.ReadU32() : reader.ReadU16();

        for (std::size_t row = 0; row < rows_; ++row)
        {
            if (counts[row] > reader.GetRemaining())
                throw std::runtime_error(
                    "ffpsd: pixel row " + std::to_string(row) + " claims " + std::to_string(counts[row]) + " bytes, only " +
                    std::to_string(reader.GetRemaining()) + " left");

            UnpackBits(bytes_.data() + reader.Tell(), counts[row], out + row * row_bytes_, row_bytes_);
            reader.Skip(counts[row]);
        }
    }

    bool PixelData::NeedsConversion(bool from_psb, bool to_psb, std::uint16_t compression) const noexcept
    {
        // Just a compression field: no rows, the same in both formats.
        if (bytes_.size() <= sizeof(std::uint16_t))
            return false;

        const std::uint16_t current = GetCompression();
        if (current == kCompressionRle && from_psb != to_psb)
            return true;
        return current != compression && sized_ && rows_ * row_bytes_ != 0 && IsRawOrRle(current) && IsRawOrRle(compression);
    }

    PixelData PixelData::Converted(bool from_psb, bool to_psb, std::uint16_t compression) const
    {
        if (!NeedsConversion(from_psb, to_psb, compression))
            return *this;
        if (!sized_)
            throw std::logic_error("ffpsd: RLE data of unknown size, such as a mask's, cannot switch between PSD and PSB yet");

        if (GetCompression() == kCompressionRle && compression == kCompressionRle)
        {
            if (std::optional<std::vector<std::uint8_t>> bytes = RewriteRowCounts(bytes_, rows_, from_psb, to_psb))
                return PixelData(std::move(*bytes), rows_, row_bytes_);
        }

        std::vector<std::uint8_t> samples(rows_ * row_bytes_);
        Decode(from_psb, samples.data());
        return Encode(samples.data(), rows_, row_bytes_, to_psb, compression);
    }

    void SwapSampleBytes(std::vector<std::uint8_t>& bytes, std::size_t sample_size) noexcept
    {
        if (sample_size == 2)
            SwapSamples<std::uint16_t>(bytes);
        else if (sample_size == 4)
            SwapSamples<std::uint32_t>(bytes);
    }
} // namespace ffpsd::detail
