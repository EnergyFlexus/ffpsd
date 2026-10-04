#ifndef FFPSD_DETAIL_IO_BIG_ENDIAN_READER_HPP_
#define FFPSD_DETAIL_IO_BIG_ENDIAN_READER_HPP_

#include "detail/io/byte_order.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // Does not own the bytes: the buffer must outlive the reader.
    class BigEndianReader
    {
    public:
        BigEndianReader() = default;
        BigEndianReader(const std::uint8_t* data, std::size_t size) noexcept;
        BigEndianReader(const std::vector<std::uint8_t>& data) noexcept;

        BigEndianReader(std::vector<std::uint8_t>&& data) = delete;

        // Copying forks the cursor.
        BigEndianReader(const BigEndianReader& other) = default;
        BigEndianReader& operator=(const BigEndianReader& other) = default;
        BigEndianReader(BigEndianReader&& other) = default;
        BigEndianReader& operator=(BigEndianReader&& other) = default;
        ~BigEndianReader() = default;

        // Reads past the end throw std::runtime_error.
        void Skip(std::size_t count);
        std::size_t Tell() const noexcept;
        std::size_t GetSize() const noexcept;
        std::size_t GetRemaining() const noexcept;
        bool AtEnd() const noexcept;

        // A 4 byte length, or 8 when wide.
        std::uint64_t ReadLength(bool wide);

        // Whether that many bytes from here end by end.
        bool FitsLength(std::uint64_t length, std::size_t end) const noexcept;

        // The length, when it fits; what names the field in the error.
        std::size_t CheckLength(std::uint64_t length, std::size_t end, const char* what) const;

        // A 4 byte length and that many bytes, which must end by end.
        std::vector<std::uint8_t> ReadBlob(std::size_t end, const char* what);

        std::uint8_t ReadU8();
        std::uint16_t ReadU16();
        std::uint32_t ReadU32();

        // None, reading nothing, when fewer than 4 bytes are left.
        std::optional<std::uint32_t> TryReadU32();
        std::uint64_t ReadU64();

        std::int16_t ReadI16();
        std::int32_t ReadI32();
        std::int64_t ReadI64();

        float ReadF32();
        double ReadF64();

        std::uint8_t PeekU8() const;
        std::uint16_t PeekU16() const;
        std::uint32_t PeekU32() const;
        std::uint64_t PeekU64() const;

        std::int16_t PeekI16() const;
        std::int32_t PeekI32() const;
        std::int64_t PeekI64() const;

        float PeekF32() const;
        double PeekF64() const;

        void ReadU8Array(std::uint8_t* dst, std::size_t count);
        void ReadU16Array(std::uint16_t* dst, std::size_t count);
        void ReadU32Array(std::uint32_t* dst, std::size_t count);
        void ReadU64Array(std::uint64_t* dst, std::size_t count);

        void ReadI16Array(std::int16_t* dst, std::size_t count);
        void ReadI32Array(std::int32_t* dst, std::size_t count);
        void ReadI64Array(std::int64_t* dst, std::size_t count);

        void ReadF32Array(float* dst, std::size_t count);
        void ReadF64Array(double* dst, std::size_t count);

    private:
        const std::uint8_t* data_ = nullptr;
        std::size_t size_ = 0;
        std::size_t offset_ = 0;

        // Data that ends early is a broken file, so this is a runtime_error.
        [[noreturn]] void ThrowPastEnd(std::size_t needed) const;

        template <typename T> T Read()
        {
            auto v = Peek<T>();
            offset_ += sizeof(T);
            return v;
        }
        template <typename T> T Peek() const
        {
            if (sizeof(T) > size_ - offset_)
                ThrowPastEnd(sizeof(T));

            T v;
            std::memcpy(&v, data_ + offset_, sizeof(T));
            return Swapped(v);
        }
        template <typename T> void ReadInto(T* dst, std::size_t count)
        {
            if (count > (size_ - offset_) / sizeof(T))
                ThrowPastEnd(count * sizeof(T));

            std::memcpy(dst, data_ + offset_, count * sizeof(T));
            offset_ += count * sizeof(T);

            if constexpr (kNativeLittle && sizeof(T) > 1)
            {
                for (std::size_t i = 0; i < count; ++i)
                    dst[i] = Swapped(dst[i]);
            }
        }
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_BIG_ENDIAN_READER_HPP_
