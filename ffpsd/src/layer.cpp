#include "detail/color.hpp"
#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"
#include "detail/layer_and_mask/adjustments/levels.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"
#include "detail/resample.hpp"

#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/layer.hpp>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ffpsd
{
    namespace
    {
        constexpr std::uint8_t kHiddenFlag = 0x02;
    } // namespace

    Layer::Layer(std::unique_ptr<detail::LayerRecord> record, Document* document) noexcept
        : record_(std::move(record))
        , document_(document)
    {
    }
    Layer::~Layer() = default;

    LayerKind Layer::GetKind() const noexcept
    {
        using detail::SectionDividerSetting;
        const std::optional<SectionDividerSetting> divider = detail::GetTaggedBlock<SectionDividerSetting>(record_->blocks);
        switch (divider.has_value() ? divider->type : SectionDividerSetting::kAnyOtherLayer)
        {
        case SectionDividerSetting::kOpenFolder:
            return LayerKind::kGroupOpen;
        case SectionDividerSetting::kClosedFolder:
            return LayerKind::kGroupClosed;
        case SectionDividerSetting::kBoundingSectionDivider:
            return LayerKind::kGroupEnd;
        default:
            return GetAdjustmentKey() != 0 ? LayerKind::kAdjustment : LayerKind::kRaster;
        }
    }

    Rect Layer::GetBounds() const noexcept
    {
        return record_->bounds;
    }

    std::string Layer::GetName() const
    {
        std::optional<detail::UnicodeLayerName> name = detail::GetTaggedBlock<detail::UnicodeLayerName>(record_->blocks);
        return name.has_value() ? std::move(name->name) : record_->name;
    }

    std::uint8_t Layer::GetOpacity() const noexcept
    {
        return record_->opacity;
    }
    bool Layer::IsVisible() const noexcept
    {
        return (record_->flags & kHiddenFlag) == 0;
    }
    bool Layer::IsBackground() const noexcept
    {
        return detail::IsBackground(*record_);
    }
    std::uint32_t Layer::GetBlendKey() const noexcept
    {
        return record_->blend_key;
    }

    void Layer::SetOpacity(std::uint8_t opacity) noexcept
    {
        record_->opacity = opacity;
    }
    void Layer::SetVisible(bool visible) noexcept
    {
        std::uint8_t& flags = record_->flags;
        flags = visible ? static_cast<std::uint8_t>(flags & ~kHiddenFlag) : static_cast<std::uint8_t>(flags | kHiddenFlag);
    }
    void Layer::SetBlendKey(std::uint32_t blend_key) noexcept
    {
        record_->blend_key = blend_key;
    }

    std::uint32_t Layer::GetAdjustmentKey() const noexcept
    {
        return detail::FindAdjustmentKey(record_->blocks);
    }

    template <class T> std::optional<T> Layer::GetAdjustment() const
    {
        const TaggedBlock* block = GetTaggedBlockByKey(T::kKey);
        if (block == nullptr)
            return std::nullopt;

        return detail::DecodeAdjustment<T>(block->data, document_->GetChannelCount());
    }

    template <class T> void Layer::SetAdjustment(const T& value)
    {
        if (GetAdjustmentKey() != T::kKey)
            throw std::invalid_argument("ffpsd: SetAdjustment needs an adjustment layer of the same kind");

        std::vector<std::uint8_t> data = detail::EncodeAdjustment(value);
        detail::FindOrAppendTaggedBlock(record_->blocks, T::kKey).data = std::move(data);
        document_->SetHasRealMergedData(false);
    }

    // A line per struct in adjustments.hpp, as in document.cpp.
    template FFPSD_EXPORT std::optional<LevelsInfo> Layer::GetAdjustment<LevelsInfo>() const;
    template FFPSD_EXPORT void Layer::SetAdjustment<LevelsInfo>(const LevelsInfo& value);

    Image Layer::GetPixels() const
    {
        return detail::DecodeLayerPixels(
            *record_, detail::LayerColorCount(document_->GetColorMode()), document_->GetDepth(), document_->IsPsb());
    }

    void Layer::SetPixels(const Image& image)
    {
        const std::uint16_t depth = document_->GetDepth();
        if (!image.IsEmpty() && image.depth != depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.depth) + " bit image in a " + std::to_string(depth) + " bit document");
        detail::CheckLayerSides(image.width, image.height, document_->IsPsb());

        detail::SamplesView samples = detail::ViewOf(image);
        samples.depth = depth; // an empty image says nothing about its depth

        detail::ReplaceLayerPixels(*record_, samples, detail::LayerColorCount(document_->GetColorMode()), document_->IsPsb());
        document_->SetHasRealMergedData(false);
    }

    void Layer::CheckTransformable(const char* what) const
    {
        if (GetKind() != LayerKind::kRaster)
            throw std::invalid_argument(std::string("ffpsd: ") + what + " needs a raster layer");
        if (detail::HasLayerMask(*record_))
            throw std::invalid_argument(std::string("ffpsd: ") + what + " of a layer with a mask is not supported yet");
    }

    void Layer::SetPosition(std::int32_t top, std::int32_t left)
    {
        const Rect bounds = record_->bounds;
        if (bounds.top == top && bounds.left == left)
            return;
        CheckTransformable("SetPosition");
        if (IsBackground())
            throw std::invalid_argument("ffpsd: the background stays at 0, 0");

        const std::int64_t bottom = std::int64_t{top} + bounds.GetHeight();
        const std::int64_t right = std::int64_t{left} + bounds.GetWidth();
        if (bottom > std::numeric_limits<std::int32_t>::max() || right > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");

        record_->bounds = {top, left, static_cast<std::int32_t>(bottom), static_cast<std::int32_t>(right)};
        document_->SetHasRealMergedData(false);
    }

    void Layer::Resize(std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        CheckTransformable("Resize");
        if (IsBackground())
            throw std::invalid_argument("ffpsd: the background keeps the canvas size");
        if (width == 0 || height == 0)
            throw std::invalid_argument("ffpsd: cannot resize a layer to " + std::to_string(width) + " x " + std::to_string(height));
        detail::CheckLayerSides(width, height, document_->IsPsb());

        const Image pixels = GetPixels();
        if (pixels.IsEmpty())
            throw std::invalid_argument("ffpsd: an empty layer has nothing to resize");

        const std::size_t color_count = detail::LayerColorCount(document_->GetColorMode());
        const Image resized = detail::Resample(pixels, width, height, color_count, filter);
        detail::ReplaceLayerPixels(*record_, detail::ViewOf(resized), color_count, document_->IsPsb());
        document_->SetHasRealMergedData(false);
    }

    std::size_t Layer::GetTaggedBlockCount() const noexcept
    {
        return record_->blocks.size();
    }
    const TaggedBlock* Layer::GetTaggedBlockByIndex(std::size_t index) const
    {
        return detail::TaggedBlockAt(record_->blocks, index);
    }
    const TaggedBlock* Layer::GetTaggedBlockByKey(std::uint32_t key) const noexcept
    {
        return detail::FindTaggedBlock(record_->blocks, key);
    }
    void Layer::SetTaggedBlock(const TaggedBlock& block)
    {
        detail::FindOrAppendTaggedBlock(record_->blocks, block.key) = block;
    }
    bool Layer::RemoveTaggedBlock(std::uint32_t key)
    {
        return detail::RemoveTaggedBlock(record_->blocks, key);
    }
} // namespace ffpsd
