#include "detail/layer_and_mask/layer_geometry.hpp"

#include "detail/composite.hpp"
#include "detail/image.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/layer_and_mask/layer_mask.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/limits.hpp"
#include "detail/resample.hpp"

#include <stdexcept>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        struct CanvasBlock
        {
            std::uint32_t key;
            const char* what;
            bool fractions; // its coordinates are fractions of the canvas size
        };

        constexpr CanvasBlock kCanvasBlocks[] = {
            {Fourcc('T', 'y', 'S', 'h'), "text", false},          {Fourcc('t', 'y', 'S', 'h'), "text", false},
            {Fourcc('S', 'o', 'L', 'd'), "smart objects", false}, {Fourcc('S', 'o', 'L', 'E'), "smart objects", false},
            {Fourcc('P', 'l', 'L', 'd'), "smart objects", false}, {Fourcc('p', 'l', 'L', 'd'), "smart objects", false},
            {Fourcc('v', 'o', 'g', 'k'), "shapes", false},        {Fourcc('v', 'm', 's', 'k'), "vector masks", true},
            {Fourcc('v', 's', 'm', 's'), "vector masks", true},
        };

        constexpr std::int16_t kMaskIds[] = {kLayerMaskId, kRealMaskId};
    } // namespace

    void CheckCanvasBlocks(const LayerRecord& record, bool keeps_fractions)
    {
        for (const CanvasBlock& block : kCanvasBlocks)
        {
            if (!(keeps_fractions && block.fractions) && FindTaggedBlock(record.blocks, block.key) != nullptr)
                throw std::logic_error(
                    "ffpsd: layer '" + record.GetName() + "' has " + block.what + ", whose placement ffpsd does not update yet");
        }
    }

    void ApplyTransformed(LayerRecord& record, TransformedLayer&& transformed)
    {
        record.bounds = transformed.bounds;
        if (transformed.channels.has_value())
            record.channels = std::move(*transformed.channels);
        record.mask_data = std::move(transformed.mask_data);
    }

    TransformedLayer TransformLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, const Transform& transform,
        ResampleFilter filter, const Rect* canvas)
    {
        TransformedLayer result;
        result.bounds = transform.Apply(record.bounds);
        result.mask_data = record.mask_data;
        for (const std::int16_t id : kMaskIds)
        {
            if (const std::optional<Rect> bounds = record.mask_data.FindBounds(id))
                result.mask_data.SetBounds(id, transform.Apply(*bounds));
        }
        if (transform.IsShift() && canvas == nullptr)
            return result;

        const auto width = static_cast<std::uint32_t>(result.bounds.GetWidth());
        const auto height = static_cast<std::uint32_t>(result.bounds.GetHeight());
        CheckLayerSides(width, height, is_psb);

        std::vector<ChannelImageData> channels;
        Image pixels = DecodeLayerPixels(record, color_mode, depth, is_psb);
        if (!pixels.IsEmpty())
        {
            const Image moved = TransformPixels(std::move(pixels), transform, width, height, filter, false);
            if (canvas != nullptr)
            {
                const auto canvas_width = static_cast<std::uint32_t>(canvas->GetWidth());
                const auto canvas_height = static_cast<std::uint32_t>(canvas->GetHeight());
                Image white = MakeWhiteImage(canvas_width, canvas_height, color_mode, depth);
                PlaceImage(white, moved, std::int64_t{result.bounds.top} - canvas->top, std::int64_t{result.bounds.left} - canvas->left);
                result.bounds = *canvas;
                channels = EncodeLayerPixels(white, color_mode, true, is_psb);
            }
            else
            {
                channels = EncodeLayerPixels(moved, color_mode, false, is_psb);
            }
        }
        else
        {
            // Groups, adjustments and empty layers: their channels are compression fields only.
            for (const ChannelImageData& channel : record.channels)
            {
                if (channel.id >= kTransparencyId)
                    channels.push_back(channel);
            }
        }

        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id >= kTransparencyId)
                continue;

            const Rect from = record.mask_data.RequireBounds(channel.id);
            const Rect to = result.mask_data.RequireBounds(channel.id);
            Image mask = channel.data.Decode(MaskInfo(from, depth), is_psb);
            if (mask.IsEmpty())
            {
                channels.push_back(channel);
                continue;
            }
            const auto mask_width = static_cast<std::uint32_t>(to.GetWidth());
            const auto mask_height = static_cast<std::uint32_t>(to.GetHeight());
            CheckLayerSides(mask_width, mask_height, is_psb);
            const Image moved = TransformPixels(std::move(mask), transform, mask_width, mask_height, filter, true);
            channels.push_back(ChannelImageData::Encode(channel.id, moved.bytes.data(), mask_width, mask_height, depth, is_psb));
        }
        result.channels = std::move(channels);
        return result;
    }
} // namespace ffpsd::detail
