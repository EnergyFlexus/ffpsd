#include "detail/io/big_endian_reader.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    BigEndianReader::BigEndianReader(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data)
        , size_(size)
    {
    }
    BigEndianReader::BigEndianReader(const std::vector<std::uint8_t>& data) noexcept
        : data_(data.data())
        , size_(data.size())
    {
    }

    void BigEndianReader::Skip(std::size_t count)
    {
        if (count > size_ - offset_)
            ThrowPastEnd(count);
        offset_ += count;
    }
    std::size_t BigEndianReader::Tell() const noexcept
    {
        return offset_;
    }
    std::size_t BigEndianReader::GetSize() const noexcept
    {
        return size_;
    }
    std::size_t BigEndianReader::GetRemaining() const noexcept
    {
        return size_ - offset_;
    }
    bool BigEndianReader::AtEnd() const noexcept
    {
        return offset_ >= size_;
    }

    void BigEndianReader::ThrowPastEnd(std::size_t needed) const
    {
        throw std::runtime_error(
            "ffpsd: need " + std::to_string(needed) + " bytes at offset " + std::to_string(offset_) + ", only " +
            std::to_string(GetRemaining()) + " left");
    }

    std::uint8_t BigEndianReader::ReadU8()
    {
        return Read<std::uint8_t>();
    }
    std::uint16_t BigEndianReader::ReadU16()
    {
        return Read<std::uint16_t>();
    }
    std::uint32_t BigEndianReader::ReadU32()
    {
        return Read<std::uint32_t>();
    }
    std::uint64_t BigEndianReader::ReadU64()
    {
        return Read<std::uint64_t>();
    }

    std::int16_t BigEndianReader::ReadI16()
    {
        return Read<std::int16_t>();
    }
    std::int32_t BigEndianReader::ReadI32()
    {
        return Read<std::int32_t>();
    }
    std::int64_t BigEndianReader::ReadI64()
    {
        return Read<std::int64_t>();
    }

    float BigEndianReader::ReadF32()
    {
        return Read<float>();
    }
    double BigEndianReader::ReadF64()
    {
        return Read<double>();
    }

    std::uint8_t BigEndianReader::PeekU8() const
    {
        return Peek<std::uint8_t>();
    }
    std::uint16_t BigEndianReader::PeekU16() const
    {
        return Peek<std::uint16_t>();
    }
    std::uint32_t BigEndianReader::PeekU32() const
    {
        return Peek<std::uint32_t>();
    }
    std::uint64_t BigEndianReader::PeekU64() const
    {
        return Peek<std::uint64_t>();
    }
    std::int16_t BigEndianReader::PeekI16() const
    {
        return Peek<std::int16_t>();
    }
    std::int32_t BigEndianReader::PeekI32() const
    {
        return Peek<std::int32_t>();
    }
    std::int64_t BigEndianReader::PeekI64() const
    {
        return Peek<std::int64_t>();
    }
    float BigEndianReader::PeekF32() const
    {
        return Peek<float>();
    }
    double BigEndianReader::PeekF64() const
    {
        return Peek<double>();
    }

    void BigEndianReader::ReadU8Array(std::uint8_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadU16Array(std::uint16_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadU32Array(std::uint32_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadU64Array(std::uint64_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadI16Array(std::int16_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadI32Array(std::int32_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadI64Array(std::int64_t* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadF32Array(float* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
    void BigEndianReader::ReadF64Array(double* dst, std::size_t count)
    {
        ReadInto(dst, count);
    }
} // namespace ffpsd::detail