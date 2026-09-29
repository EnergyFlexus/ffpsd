#include "detail/io/big_endian_writer.hpp"

#include <limits>
#include <string>
#include <utility>

namespace ffpsd::detail
{
    BigEndianWriter::BigEndianWriter(std::size_t reserve_bytes)
    {
        data_.reserve(reserve_bytes);
    }

    void BigEndianWriter::WriteU8(std::uint8_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteU16(std::uint16_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteU32(std::uint32_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteU64(std::uint64_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteI16(std::int16_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteI32(std::int32_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteI64(std::int64_t value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteF32(float value)
    {
        Write(value);
    }
    void BigEndianWriter::WriteF64(double value)
    {
        Write(value);
    }

    void BigEndianWriter::WriteU8Array(const std::uint8_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteU16Array(const std::uint16_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteU32Array(const std::uint32_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteU64Array(const std::uint64_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteI16Array(const std::int16_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteI32Array(const std::int32_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteI64Array(const std::int64_t* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteF32Array(const float* src, std::size_t count)
    {
        WriteArray(src, count);
    }
    void BigEndianWriter::WriteF64Array(const double* src, std::size_t count)
    {
        WriteArray(src, count);
    }

    void BigEndianWriter::WriteZeros(std::size_t count)
    {
        data_.insert(data_.end(), count, std::uint8_t{0});
    }
    void BigEndianWriter::PadFrom(std::size_t start, std::size_t alignment)
    {
        const std::size_t written = data_.size() - start;
        WriteZeros((alignment - written % alignment) % alignment);
    }

    std::size_t BigEndianWriter::ReserveU32()
    {
        const std::size_t position = data_.size();
        WriteU32(0);
        return position;
    }
    std::size_t BigEndianWriter::ReserveU64()
    {
        const std::size_t position = data_.size();
        WriteU64(0);
        return position;
    }
    void BigEndianWriter::PatchU32(std::size_t position, std::uint32_t value)
    {
        Patch(position, value);
    }
    void BigEndianWriter::PatchU64(std::size_t position, std::uint64_t value)
    {
        Patch(position, value);
    }

    std::size_t BigEndianWriter::ReserveLength(bool wide)
    {
        return wide ? ReserveU64() : ReserveU32();
    }
    void BigEndianWriter::PatchLength(std::size_t position, bool wide)
    {
        const std::size_t field = wide ? sizeof(std::uint64_t) : sizeof(std::uint32_t);
        const std::uint64_t length = data_.size() - position - field;
        if (wide)
        {
            PatchU64(position, length);
            return;
        }
        if (length > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("ffpsd: " + std::to_string(length) + " bytes do not fit a 4 byte length");
        PatchU32(position, static_cast<std::uint32_t>(length));
    }

    std::size_t BigEndianWriter::Tell() const noexcept
    {
        return data_.size();
    }
    const std::vector<std::uint8_t>& BigEndianWriter::GetBytes() const noexcept
    {
        return data_;
    }
    std::vector<std::uint8_t> BigEndianWriter::Take() noexcept
    {
        return std::move(data_);
    }
} // namespace ffpsd::detail
