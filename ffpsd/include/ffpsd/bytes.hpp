#ifndef FFPSD_BYTES_HPP_
#define FFPSD_BYTES_HPP_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ffpsd/export.h>
#include <initializer_list>
#include <memory>
#include <utility>

namespace ffpsd
{
    // Frees in the library, so memory it allocated goes back to the heap it came from, whichever side lets it go.
    struct BytesDeleter
    {
        FFPSD_EXPORT void operator()(std::uint8_t* data) const noexcept;
    };

    // Owned bytes allocated by the library; unlike std::vector, a new size is not zeroed first.
    class Bytes
    {
    public:
        using value_type = std::uint8_t;
        using iterator = std::uint8_t*;
        using const_iterator = const std::uint8_t*;

        Bytes() = default;

        // Left as memory comes: every byte is the caller's to write.
        FFPSD_EXPORT explicit Bytes(std::size_t size);
        FFPSD_EXPORT Bytes(std::size_t size, std::uint8_t value);
        FFPSD_EXPORT Bytes(const std::uint8_t* data, std::size_t size);
        Bytes(std::initializer_list<std::uint8_t> values)
            : Bytes(values.begin(), values.size())
        {
        }

        FFPSD_EXPORT Bytes(const Bytes& other);
        FFPSD_EXPORT Bytes& operator=(const Bytes& other);
        Bytes(Bytes&& other) noexcept
            : data_(std::move(other.data_))
            , size_(std::exchange(other.size_, 0))
        {
        }
        Bytes& operator=(Bytes&& other) noexcept
        {
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0);
            return *this;
        }
        ~Bytes() = default;

        std::uint8_t* data() noexcept
        {
            return data_.get();
        }
        const std::uint8_t* data() const noexcept
        {
            return data_.get();
        }
        std::size_t size() const noexcept
        {
            return size_;
        }
        bool empty() const noexcept
        {
            return size_ == 0;
        }

        std::uint8_t& operator[](std::size_t index) noexcept
        {
            return data_[index];
        }
        std::uint8_t operator[](std::size_t index) const noexcept
        {
            return data_[index];
        }

        iterator begin() noexcept
        {
            return data_.get();
        }
        iterator end() noexcept
        {
            return data_.get() + size_;
        }
        const_iterator begin() const noexcept
        {
            return data_.get();
        }
        const_iterator end() const noexcept
        {
            return data_.get() + size_;
        }

        friend bool operator==(const Bytes& x, const Bytes& y) noexcept
        {
            return x.size_ == y.size_ && (x.size_ == 0 || std::memcmp(x.data(), y.data(), x.size_) == 0);
        }
        friend bool operator!=(const Bytes& x, const Bytes& y) noexcept
        {
            return !(x == y);
        }

    private:
        std::unique_ptr<std::uint8_t[], BytesDeleter> data_;
        std::size_t size_ = 0;
    };
} // namespace ffpsd

#endif // FFPSD_BYTES_HPP_
