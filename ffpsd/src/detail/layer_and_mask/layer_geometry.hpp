#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_

#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/layer_record.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // x' = a·x + c·y + tx, y' = b·x + d·y + ty; only scales, mirrors and quarter turns, which keep rectangles upright.
    struct Transform
    {
        double a = 1;
        double b = 0;
        double c = 0;
        double d = 1;
        double tx = 0;
        double ty = 0;

        static Transform Shift(double dx, double dy) noexcept;
        static Transform Scale(double sx, double sy, double origin_x, double origin_y) noexcept;

        // The frame turns in place: its top left corner stays.
        static Transform Rotate(Rotation rotation, const Rect& frame) noexcept;
        static Transform Flip(FlipDirection direction, const Rect& frame) noexcept;

        Transform Then(const Transform& next) const noexcept;

        bool SwapsAxes() const noexcept
        {
            return a == 0;
        }
        bool IsShift() const noexcept
        {
            return a == 1 && b == 0 && c == 0 && d == 1;
        }
    };

    // Anything with pixels keeps one at least.
    Rect TransformRect(const Rect& rect, const Transform& transform);

    // Turned, then resampled to width and height; planes_alone keeps alpha and spot channels from counting as transparency.
    Image TransformPixels(
        Image image, const Transform& transform, std::uint32_t width, std::uint32_t height, ResampleFilter filter, bool planes_alone);

    // Vector mask points are fractions of the canvas, so they stay right only while the canvas scales with them.
    void CheckCanvasBlocks(const LayerRecord& record, bool keeps_fractions);

    struct TransformedLayer
    {
        Rect bounds;
        std::optional<std::vector<ChannelImageData>> channels; // none: the samples stay as they are
        std::vector<std::uint8_t> mask_data;
    };

    // Cannot throw, so it runs once everything that can is done.
    void ApplyTransformed(LayerRecord& record, TransformedLayer&& transformed);

    // A background is cut or padded with white to canvas.
    TransformedLayer TransformLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, const Transform& transform,
        ResampleFilter filter, const Rect* canvas = nullptr);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_GEOMETRY_HPP_
