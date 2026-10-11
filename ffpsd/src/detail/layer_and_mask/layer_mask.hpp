#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_MASK_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_MASK_HPP_

#include "detail/layer_and_mask/layer_record.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/types.hpp>
#include <optional>

namespace ffpsd::detail
{
    // The shape of a mask over bounds, gray; empty when they cover nothing.
    ImageInfo MaskInfo(const Rect& bounds, std::uint16_t depth) noexcept;

    std::optional<LayerMask> DecodeLayerMask(const LayerRecord& record, std::uint16_t depth, bool is_psb);

    // As layer -2; the caller checks the image and that there is no vector mask.
    void ReplaceLayerMask(
        LayerRecord& record, const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_MASK_HPP_
