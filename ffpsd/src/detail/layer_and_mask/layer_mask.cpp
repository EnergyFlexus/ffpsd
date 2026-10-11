#include "detail/layer_and_mask/layer_mask.hpp"

#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"

#include <utility>

namespace ffpsd::detail
{
    ImageInfo MaskInfo(const Rect& bounds, std::uint16_t depth) noexcept
    {
        if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0)
            return ImageInfo{0, 0, 1, depth, ColorMode::kGrayscale};
        return ImageInfo{
            static_cast<std::uint32_t>(bounds.GetWidth()), static_cast<std::uint32_t>(bounds.GetHeight()), 1, depth, ColorMode::kGrayscale};
    }

    std::optional<LayerMask> DecodeLayerMask(const LayerRecord& record, std::uint16_t depth, bool is_psb)
    {
        const std::optional<std::int16_t> id = record.FindPixelMaskId();
        if (!id.has_value())
            return std::nullopt;

        LayerMask mask;
        mask.bounds = record.mask_data.RequireBounds(*id);
        mask.default_color = record.mask_data.FindDefaultColor(*id).value_or(0);
        mask.image = record.FindChannel(*id)->data.Decode(MaskInfo(mask.bounds, depth), is_psb);
        return mask;
    }

    void ReplaceLayerMask(
        LayerRecord& record, const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color, bool is_psb)
    {
        const Rect bounds = BoundsAt(image, top, left);
        MaskData mask_data = record.mask_data.IsEmpty() ? MaskData::BlankLayerMask() : record.mask_data;
        mask_data.SetBounds(kLayerMaskId, bounds);
        mask_data.SetDefaultColor(kLayerMaskId, default_color);
        ChannelImageData channel = image.IsEmpty()
                                       ? ChannelImageData::Empty(kLayerMaskId)
                                       : ChannelImageData::Encode(kLayerMaskId, image.data, image.width, image.height, image.depth, is_psb);
        record.RemoveChannels(kLayerMaskId);
        record.channels.push_back(std::move(channel));
        record.mask_data = std::move(mask_data);
    }
} // namespace ffpsd::detail
