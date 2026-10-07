#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // Channel -2 or -3 among the layer's channels.
    bool HasLayerMask(const LayerRecord& record) noexcept;

    bool HasVectorMask(const LayerRecord& record) noexcept;

    // The channel of the pixel mask, 0 when there is none; a -2 rendered from a vector mask is not one.
    std::int16_t FindPixelMaskId(const LayerRecord& record) noexcept;

    Image DecodeMask(const ChannelImageData& channel, const Rect& bounds, std::uint16_t depth, bool is_psb);

    std::optional<LayerMask> DecodeLayerMask(const LayerRecord& record, std::uint16_t depth, bool is_psb);

    // As layer -2; the caller checks the image and that there is no vector mask.
    void ReplaceLayerMask(
        LayerRecord& record, const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color, bool is_psb);

    // A layer may be no wider or taller than the document could be.
    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb);

    // An empty image or one of the document's mode and depth, colors and at most transparency; returned in that mode.
    ImageView CheckLayerImage(const ImageView& image, ColorMode color_mode, std::uint16_t depth, bool is_psb);

    // Transparency first, opaque when the image has none unless it is the background's; then the color planes.
    std::vector<ChannelImageData> EncodeLayerPixels(const ImageView& image, bool is_background, bool is_psb);

    // The rectangle the image covers at top and left; throws when it leaves the 32 bit range.
    Rect BoundsAt(const ImageView& image, std::int32_t top, std::int32_t left);

    // The color planes by channel id, then transparency when the layer has it; masks stay out.
    Image DecodeLayerPixels(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb);

    // The shape DecodeLayerPixels gives, without decoding; the same into out, exactly its GetSizeBytes.
    ImageInfo LayerPixelsInfo(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth);
    void DecodeLayerPixels(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, std::uint8_t* out, std::size_t size);

    // New color and transparency channels at the same top left corner; the masks stay.
    void ReplaceLayerPixels(LayerRecord& record, const ImageView& image, bool is_background, bool is_psb);

    // Color planes first; one plane more is transparency. An empty image gives empty channels of its mode.
    LayerRecord CreateLayerRecord(
        const std::string& name, const ImageView& image, std::int32_t top, std::int32_t left, bool is_background, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
