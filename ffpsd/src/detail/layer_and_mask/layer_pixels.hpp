#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // Borrowed planar samples in native byte order; the caller keeps them alive for the call.
    struct SamplesView
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;
    };

    // A view of the image's own bytes, valid while the image lives.
    SamplesView ViewOf(const Image& image) noexcept;

    // Channel -2 or -3 among the layer's channels.
    bool HasLayerMask(const LayerRecord& record) noexcept;

    // A layer may be no wider or taller than the document could be.
    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb);

    // Transparency first, then the color planes; no samples give empty channels.
    std::vector<ChannelImageData> EncodeLayerPixels(const SamplesView& samples, std::size_t color_count, bool is_psb);

    // The rectangle the samples cover at top and left; throws when it leaves the 32 bit range.
    Rect BoundsAt(const SamplesView& samples, std::int32_t top, std::int32_t left);

    // The color planes by channel id, then transparency when the layer has it; masks stay out.
    Image DecodeLayerPixels(const LayerRecord& record, std::size_t color_count, std::uint16_t depth, bool is_psb);

    // New color and transparency channels at the same top left corner; the masks stay.
    void ReplaceLayerPixels(LayerRecord& record, const SamplesView& samples, std::size_t color_count, bool is_psb);

    // Color planes first; one plane more is transparency. No samples give empty channels.
    LayerRecord CreateLayerRecord(
        const std::string& name, const SamplesView& samples, std::int32_t top, std::int32_t left, std::size_t color_count, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_PIXELS_HPP_
