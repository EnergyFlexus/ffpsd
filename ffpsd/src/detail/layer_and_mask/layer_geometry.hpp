#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstdint>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <vector>

namespace ffpsd::detail
{
    // Vector mask points are fractions of the canvas size, so only scaling leaves them right.
    void CheckCanvasBlocks(const LayerRecord& record, bool scaled);

    Rect ShiftRect(const Rect& rect, std::int64_t dy, std::int64_t dx);
    void ShiftMaskBounds(std::vector<std::uint8_t>& mask_data, std::int64_t dy, std::int64_t dx);

    struct ScaledLayer
    {
        Rect bounds;
        std::vector<ChannelImageData> channels;
        std::vector<std::uint8_t> mask_data;
    };

    // Edges keep their distance from the origin times the factor.
    ScaledLayer ScaleLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, bool is_background, double sy, double sx,
        std::int64_t origin_top, std::int64_t origin_left, ResampleFilter filter);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
