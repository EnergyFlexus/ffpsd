#ifndef FFPSD_DETAIL_IO_BIG_ENDIAN_WRITER_HPP_
#define FFPSD_DETAIL_IO_BIG_ENDIAN_WRITER_HPP_

#include "detail/io/byte_order.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ffpsd::detail
{
    class BigEndianWriter
    {
    public:
        BigEndianWriter() = default;
        explicit BigEndianWriter(std::size_t reserve_bytes);

        BigEndianWriter(BigEndianWriter&& other) = default;
        BigEndianWriter& operator=(BigEndianWriter&& other) = default;
        ~BigEndianWriter() = default;

        BigEndianWriter(const BigEndianWriter& other) = delete;
        BigEndianWriter& operator=(const BigEndianWriter& other) = delete;

        void WriteU8(std::uint8_t value);
        void WriteU16(std::uint16_t value);
        void WriteU32(std::uint32_t value);
        void WriteU64(std::uint64_t value);

        void WriteI16(std::int16_t value);
        void WriteI32(std::int32_t value);
        void WriteI64(std::int64_t value);

        void WriteF32(float value);
        void WriteF64(double value);

        void WriteU8Array(const std::uint8_t* src, std::size_t count);
        void WriteU16Array(const std::uint16_t* src, std::size_t count);
        void WriteU32Array(const std::uint32_t* src, std::size_t count);
        void WriteU64Array(const std::uint64_t* src, std::size_t count);

        void WriteI16Array(const std::int16_t* src, std::size_t count);
        void WriteI32Array(const std::int32_t* src, std::size_t count);
        void WriteI64Array(const std::int64_t* src, std::size_t count);

        void WriteF32Array(const float* src, std::size_t count);
        void WriteF64Array(const double* src, std::size_t count);

        void WriteZeros(std::size_t count);

        // Pads with zeros until the distance from start is a multiple of it.
        void PadFrom(std::size_t start, std::size_t alignment);

        std::size_t ReserveU32();
        std::size_t ReserveU64();
        void PatchU32(std::size_t position, std::uint32_t value);
        void PatchU64(std::size_t position, std::uint64_t value);

        // A length field of 4 bytes, or 8 when wide, patched with the bytes written after it.
        std::size_t ReserveLength(bool wide);
        void PatchLength(std::size_t position, bool wide);

        std::size_t Tell() const noexcept;
        const std::vector<std::uint8_t>& GetBytes() const noexcept;
        std::vector<std::uint8_t> Take() noexcept;

    private:
        std::vector<std::uint8_t> data_;

        template <typename T> void Write(T value)
        {
            const T raw = Swapped(value);
            const auto* first = reinterpret_cast<const std::uint8_t*>(&raw);
            data_.insert(data_.end(), first, first + sizeof(T));
        }

        template <typename T> void WriteArray(const T* src, std::size_t count)
        {
            if (count > (data_.max_size() - data_.size()) / sizeof(T))
                throw std::length_error("ffpsd: array too large to write");

            const std::size_t at = data_.size();
            data_.resize(at + count * sizeof(T));
            std::memcpy(data_.data() + at, src, count * sizeof(T));

            if constexpr (kNativeLittle && sizeof(T) > 1)
            {
                for (std::size_t i = 0; i < count; ++i)
                {
                    T value;
                    std::memcpy(&value, data_.data() + at + i * sizeof(T), sizeof(T));
                    value = Swapped(value);
                    std::memcpy(data_.data() + at + i * sizeof(T), &value, sizeof(T));
                }
            }
        }

        template <typename T> void Patch(std::size_t position, T value)
        {
            if (position + sizeof(T) > data_.size())
                throw std::out_of_range("ffpsd: patch position outside the buffer");

            const T raw = Swapped(value);
            std::memcpy(data_.data() + position, &raw, sizeof(T));
        }
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_BIG_ENDIAN_WRITER_HPP_
