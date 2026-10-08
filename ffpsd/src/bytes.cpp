#include <cstring>
#include <ffpsd/bytes.hpp>

namespace ffpsd
{
    void BytesDeleter::operator()(std::uint8_t* data) const noexcept
    {
        delete[] data;
    }

    // No () after the brackets: the bytes stay as the allocator gives them.
    Bytes::Bytes(std::size_t size)
        : data_(size == 0 ? nullptr : new std::uint8_t[size])
        , size_(size)
    {
    }

    Bytes::Bytes(std::size_t size, std::uint8_t value)
        : Bytes(size)
    {
        if (size_ != 0)
            std::memset(data_.get(), value, size_);
    }

    Bytes::Bytes(const std::uint8_t* data, std::size_t size)
        : Bytes(size)
    {
        if (size_ != 0)
            std::memcpy(data_.get(), data, size_);
    }

    Bytes::Bytes(const Bytes& other)
        : Bytes(other.data(), other.size())
    {
    }

    Bytes& Bytes::operator=(const Bytes& other)
    {
        if (this != &other)
            *this = Bytes(other);
        return *this;
    }
} // namespace ffpsd
