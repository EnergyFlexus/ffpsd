#include "detail/pixel_data.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/byte_order.hpp"
#include "detail/io/compression.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

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
    } // namespace

    void
    DecodePixelData(const std::uint8_t* data, std::size_t size, std::size_t rows, std::size_t row_bytes, bool is_psb, std::uint8_t* out)
    {
        BigEndianReader reader(data, size);
        const std::uint16_t compression = reader.ReadU16();
        const std::size_t total = rows * row_bytes;

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
        std::vector<std::size_t> counts(rows);
        for (std::size_t& count : counts)
            count = is_psb ? reader.ReadU32() : reader.ReadU16();

        for (std::size_t row = 0; row < rows; ++row)
        {
            if (counts[row] > reader.GetRemaining())
                throw std::runtime_error(
                    "ffpsd: pixel row " + std::to_string(row) + " claims " + std::to_string(counts[row]) + " bytes, only " +
                    std::to_string(reader.GetRemaining()) + " left");

            UnpackBits(data + reader.Tell(), counts[row], out + row * row_bytes, row_bytes);
            reader.Skip(counts[row]);
        }
    }

    std::vector<std::uint8_t>
    EncodePixelData(const std::uint8_t* data, std::size_t rows, std::size_t row_bytes, bool is_psb, std::uint16_t compression)
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
        if (packs)
            return out;

        out.assign(header, 0);
        out.insert(out.end(), data, data + total);
        return out;
    }

    std::vector<std::uint8_t> ConvertPixelData(
        const std::vector<std::uint8_t>& data, std::size_t rows, std::size_t row_bytes, bool from_psb, bool to_psb,
        std::uint16_t compression)
    {
        std::vector<std::uint8_t> samples(rows * row_bytes);
        DecodePixelData(data.data(), data.size(), rows, row_bytes, from_psb, samples.data());
        return EncodePixelData(samples.data(), rows, row_bytes, to_psb, compression);
    }

    std::uint16_t GetPixelCompression(const std::vector<std::uint8_t>& data) noexcept
    {
        if (data.size() < sizeof(std::uint16_t))
            return kCompressionRaw;
        return static_cast<std::uint16_t>(data[0] << 8 | data[1]);
    }

    bool IsRlePixelData(const std::vector<std::uint8_t>& data) noexcept
    {
        return GetPixelCompression(data) == kCompressionRle;
    }

    void SwapSampleBytes(std::vector<std::uint8_t>& bytes, std::size_t sample_size) noexcept
    {
        if (sample_size == 2)
            SwapSamples<std::uint16_t>(bytes);
        else if (sample_size == 4)
            SwapSamples<std::uint32_t>(bytes);
    }
} // namespace ffpsd::detail
