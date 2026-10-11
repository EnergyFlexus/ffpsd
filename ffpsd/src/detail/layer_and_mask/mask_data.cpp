#include "detail/layer_and_mask/mask_data.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/rect.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        // Bit 4 of the mask flags: density and feather follow, as the parameter flags pick them.
        constexpr std::uint8_t kMaskHasParameters = 0x10;

        // User mask density and feather, then vector mask density and feather, by bits 0 to 3.
        constexpr std::size_t kMaskParameterSizes[] = {1, 8, 1, 8};

        constexpr std::size_t kRectSize = 16;

        // Bit 3 of the mask flags.
        constexpr std::uint8_t kMaskFromRender = 0x08;
        constexpr std::size_t kMaskDataSize = 20;
    } // namespace

    MaskData MaskData::BlankLayerMask()
    {
        return MaskData(std::vector<std::uint8_t>(kMaskDataSize, 0));
    }

    std::optional<Rect> MaskData::FindBounds(std::int16_t id) const
    {
        const std::optional<std::size_t> offset = FindRectOffset(id);
        if (!offset.has_value())
            return std::nullopt;

        BigEndianReader reader(bytes_);
        reader.Skip(*offset);
        return ReadRect(reader);
    }

    Rect MaskData::RequireBounds(std::int16_t id) const
    {
        const std::optional<Rect> bounds = FindBounds(id);
        if (!bounds.has_value())
            throw std::runtime_error("ffpsd: the mask data has no rectangle for mask " + std::to_string(id));
        return *bounds;
    }

    void MaskData::SetBounds(std::int16_t id, const Rect& bounds)
    {
        const std::optional<std::size_t> offset = FindRectOffset(id);
        if (!offset.has_value())
            throw std::runtime_error("ffpsd: the mask data has no rectangle for mask " + std::to_string(id));

        BigEndianWriter writer(kRectSize);
        WriteRect(writer, bounds);
        std::copy(writer.GetBytes().begin(), writer.GetBytes().end(), bytes_.begin() + static_cast<std::ptrdiff_t>(*offset));
    }

    std::optional<std::uint8_t> MaskData::FindDefaultColor(std::int16_t id) const
    {
        const std::optional<std::size_t> offset = FindDefaultColorOffset(id);
        return offset.has_value() ? std::optional<std::uint8_t>(bytes_[*offset]) : std::nullopt;
    }

    void MaskData::SetDefaultColor(std::int16_t id, std::uint8_t color)
    {
        const std::optional<std::size_t> offset = FindDefaultColorOffset(id);
        if (!offset.has_value())
            throw std::runtime_error("ffpsd: the mask data has no default color for mask " + std::to_string(id));
        bytes_[*offset] = color;
    }

    bool MaskData::IsRendered() const noexcept
    {
        return bytes_.size() >= kRectSize + 2 && (bytes_[kRectSize + 1] & kMaskFromRender) != 0;
    }

    std::optional<std::size_t> MaskData::FindRectOffset(std::int16_t id) const
    {
        // The layer mask's rectangle, its default color and flags; the size of 20 pads them.
        BigEndianReader reader(bytes_);
        if (reader.GetRemaining() < kRectSize + 2)
            return std::nullopt;
        if (id == kLayerMaskId)
            return 0;
        if (id != kRealMaskId)
            return std::nullopt;

        reader.Skip(kRectSize + 1);
        const std::uint8_t flags = reader.ReadU8();
        if ((flags & kMaskHasParameters) != 0)
        {
            if (reader.GetRemaining() < 1)
                return std::nullopt;
            const std::uint8_t parameters = reader.ReadU8();
            std::size_t size = 0;
            for (std::size_t bit = 0; bit < std::size(kMaskParameterSizes); ++bit)
            {
                if ((parameters >> bit & 1) != 0)
                    size += kMaskParameterSizes[bit];
            }
            if (reader.GetRemaining() < size)
                return std::nullopt;
            reader.Skip(size);
        }

        // The real user mask's flags and background, then its rectangle.
        if (reader.GetRemaining() < 2 + kRectSize)
            return std::nullopt;
        return reader.Tell() + 2;
    }

    std::optional<std::size_t> MaskData::FindDefaultColorOffset(std::int16_t id) const
    {
        const std::optional<std::size_t> rect = FindRectOffset(id);
        if (!rect.has_value())
            return std::nullopt;
        return id == kLayerMaskId ? *rect + kRectSize : *rect - 1;
    }
} // namespace ffpsd::detail
