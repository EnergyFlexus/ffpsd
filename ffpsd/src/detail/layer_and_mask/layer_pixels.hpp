#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd::detail
{
    // Transparency first, opaque when the image has none unless it is the background's; then the color planes.
    std::vector<ChannelImageData> EncodeLayerPixels(const ImageView& image, ColorMode color_mode, bool is_background, bool is_psb);

    // The rectangle the image covers at top and left; throws when it leaves the 32 bit range.
    Rect BoundsAt(const ImageView& image, std::int32_t top, std::int32_t left);

    // The color planes by channel id, then transparency when the layer has it; masks stay out.
    Image DecodeLayerPixels(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb);
    void DecodeLayerPixels(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, std::uint8_t* out, std::size_t size);

    // The shape DecodeLayerPixels gives, without decoding; the same into out, exactly its GetSizeBytes.
    ImageInfo LayerPixelsInfo(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth);

    // New color and transparency channels at the same top left corner; the masks stay.
    void ReplaceLayerPixels(LayerRecord& record, const ImageView& image, ColorMode color_mode, bool is_background, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
