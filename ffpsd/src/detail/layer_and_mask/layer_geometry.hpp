#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/transform.hpp"

#include <cstdint>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // Vector mask points are fractions of the canvas, so they stay right only while the canvas scales with them.
    void CheckCanvasBlocks(const LayerRecord& record, bool keeps_fractions);

    struct TransformedLayer
    {
        Rect bounds;
        std::optional<std::vector<ChannelImageData>> channels; // none: the samples stay as they are
        MaskData mask_data;
    };

    // Cannot throw, so it runs once everything that can is done.
    void ApplyTransformed(LayerRecord& record, TransformedLayer&& transformed);

    // A background is cut or padded with white to canvas.
    TransformedLayer TransformLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, const Transform& transform,
        ResampleFilter filter, const Rect* canvas = nullptr);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
