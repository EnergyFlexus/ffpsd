#include "detail/layer_and_mask/layer_geometry.hpp"

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/pixel_data.hpp"
#include "detail/resample.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        struct CanvasBlock
        {
            std::uint32_t key;
            const char* what;
            bool scales; // its coordinates are fractions of the canvas size
        };

        constexpr CanvasBlock kCanvasBlocks[] = {
            {Fourcc('T', 'y', 'S', 'h'), "text", false},          {Fourcc('t', 'y', 'S', 'h'), "text", false},
            {Fourcc('S', 'o', 'L', 'd'), "smart objects", false}, {Fourcc('S', 'o', 'L', 'E'), "smart objects", false},
            {Fourcc('P', 'l', 'L', 'd'), "smart objects", false}, {Fourcc('p', 'l', 'L', 'd'), "smart objects", false},
            {Fourcc('v', 'o', 'g', 'k'), "shapes", false},        {Fourcc('v', 'm', 's', 'k'), "vector masks", true},
            {Fourcc('v', 's', 'm', 's'), "vector masks", true},
        };

        constexpr std::int16_t kMaskIds[] = {kLayerMaskId, kRealMaskId};

        Rect MakeRect(std::int64_t top, std::int64_t left, std::int64_t bottom, std::int64_t right)
        {
            constexpr std::int64_t kLowest = std::numeric_limits<std::int32_t>::min();
            constexpr std::int64_t kHighest = std::numeric_limits<std::int32_t>::max();
            for (const std::int64_t edge : {top, left, bottom, right})
            {
                if (edge < kLowest || edge > kHighest)
                    throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
            }
            return {
                static_cast<std::int32_t>(top), static_cast<std::int32_t>(left), static_cast<std::int32_t>(bottom),
                static_cast<std::int32_t>(right)};
        }

        // Anything with pixels keeps one at least.
        Rect ScaleRect(const Rect& rect, double sy, double sx, std::int64_t origin_top, std::int64_t origin_left)
        {
            const auto scale = [](std::int64_t edge, std::int64_t origin, double factor) {
                return origin + std::llround(static_cast<double>(edge - origin) * factor);
            };
            const std::int64_t top = scale(rect.top, origin_top, sy);
            const std::int64_t left = scale(rect.left, origin_left, sx);
            const std::int64_t bottom = rect.GetHeight() > 0 ? std::max(scale(rect.bottom, origin_top, sy), top + 1) : top;
            const std::int64_t right = rect.GetWidth() > 0 ? std::max(scale(rect.right, origin_left, sx), left + 1) : left;
            return MakeRect(top, left, bottom, right);
        }
    } // namespace

    void CheckCanvasBlocks(const LayerRecord& record, bool scaled)
    {
        for (const CanvasBlock& block : kCanvasBlocks)
        {
            if (!(scaled && block.scales) && FindTaggedBlock(record.blocks, block.key) != nullptr)
                throw std::invalid_argument(
                    std::string("ffpsd: layer '") + record.name + "' has " + block.what + ", which a canvas change does not update yet");
        }
    }

    Rect ShiftRect(const Rect& rect, std::int64_t dy, std::int64_t dx)
    {
        return MakeRect(rect.top + dy, rect.left + dx, rect.bottom + dy, rect.right + dx);
    }

    void ShiftMaskBounds(std::vector<std::uint8_t>& mask_data, std::int64_t dy, std::int64_t dx)
    {
        for (const std::int16_t id : kMaskIds)
        {
            if (const std::optional<Rect> bounds = FindMaskBounds(mask_data, id))
                SetMaskBounds(mask_data, id, ShiftRect(*bounds, dy, dx));
        }
    }

    ScaledLayer ScaleLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, bool is_background, double sy, double sx,
        std::int64_t origin_top, std::int64_t origin_left, ResampleFilter filter)
    {
        ScaledLayer scaled;
        scaled.bounds = ScaleRect(record.bounds, sy, sx, origin_top, origin_left);
        const auto width = static_cast<std::uint32_t>(scaled.bounds.GetWidth());
        const auto height = static_cast<std::uint32_t>(scaled.bounds.GetHeight());
        CheckLayerSides(width, height, is_psb);

        const Image pixels = DecodeLayerPixels(record, color_mode, depth, is_psb);
        if (!pixels.IsEmpty())
        {
            scaled.channels = EncodeLayerPixels(Resample(pixels, width, height, filter), is_background, is_psb);
        }
        else
        {
            // Groups, adjustments and empty layers: their channels are compression fields only.
            for (const ChannelImageData& channel : record.channels)
            {
                if (channel.id >= kTransparencyId)
                    scaled.channels.push_back(channel);
            }
        }

        scaled.mask_data = record.mask_data;
        for (const std::int16_t id : kMaskIds)
        {
            if (const std::optional<Rect> bounds = FindMaskBounds(record.mask_data, id))
                SetMaskBounds(scaled.mask_data, id, ScaleRect(*bounds, sy, sx, origin_top, origin_left));
        }

        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id >= kTransparencyId)
                continue;

            const std::optional<Rect> from = FindMaskBounds(record.mask_data, channel.id);
            const std::optional<Rect> to = FindMaskBounds(scaled.mask_data, channel.id);
            if (!from.has_value() || !to.has_value())
                throw std::runtime_error(
                    "ffpsd: layer '" + record.name + "' has mask channel " + std::to_string(channel.id) + " without its rectangle");

            const Image mask = DecodeMask(channel, *from, depth, is_psb);
            if (mask.IsEmpty())
            {
                scaled.channels.push_back(channel);
                continue;
            }
            const auto mask_width = static_cast<std::uint32_t>(to->GetWidth());
            const auto mask_height = static_cast<std::uint32_t>(to->GetHeight());
            CheckLayerSides(mask_width, mask_height, is_psb);
            const Image resized = Resample(mask, mask_width, mask_height, filter);
            scaled.channels.push_back(
                EncodeChannelImageData(channel.id, resized.bytes.data(), mask_width, mask_height, SampleBytes(depth), is_psb));
        }
        return scaled;
    }
} // namespace ffpsd::detail
