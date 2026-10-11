#ifndef FFPSD_DETAIL_LAYER_AND_MASK_MASK_DATA_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_MASK_DATA_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/types.hpp>
#include <optional>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    // A layer record's mask data as stored: only the rectangles and default colors are read, the rest stays as bytes.
    class MaskData
    {
    public:
        MaskData() = default;
        explicit MaskData(std::vector<std::uint8_t> bytes) noexcept
            : bytes_(std::move(bytes))
        {
        }

        // Just a layer mask: its rectangle, default color, flags and padding.
        static MaskData BlankLayerMask();

        const std::vector<std::uint8_t>& GetBytes() const noexcept
        {
            return bytes_;
        }
        bool IsEmpty() const noexcept
        {
            return bytes_.empty();
        }
        void Clear() noexcept
        {
            bytes_.clear();
        }

        // The rectangle mask channel -2 or -3 covers; none when the data has no such one.
        std::optional<Rect> FindBounds(std::int16_t id) const;

        // For a mask channel the record has: data without its rectangle is a damaged file.
        Rect RequireBounds(std::int16_t id) const;

        // Throws when the data has no rectangle for that mask.
        void SetBounds(std::int16_t id, const Rect& bounds);

        // The value everywhere outside the rectangle, 0 or 255.
        std::optional<std::uint8_t> FindDefaultColor(std::int16_t id) const;
        void SetDefaultColor(std::int16_t id, std::uint8_t color);

        // Then -2 holds Photoshop's rendering of a vector mask, and the pixel mask is -3.
        bool IsRendered() const noexcept;

    private:
        std::optional<std::size_t> FindRectOffset(std::int16_t id) const;

        // After the layer mask's rectangle, but before the real user mask's.
        std::optional<std::size_t> FindDefaultColorOffset(std::int16_t id) const;

        std::vector<std::uint8_t> bytes_;
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_MASK_DATA_HPP_
