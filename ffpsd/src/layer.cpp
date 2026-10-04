#include "detail/image.hpp"
#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"
#include "detail/layer_and_mask/adjustments/levels.hpp"
#include "detail/layer_and_mask/layer_geometry.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"
#include "detail/resample.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/layer.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
        return document_->GetLayerCount() != 0 && document_->GetLayerByIndex(0) == this && GetKind() == LayerKind::kRaster &&
               detail::HasBackgroundMarks(*record_);
    }
    std::uint32_t Layer::GetBlendKey() const noexcept
    {
        return record_->blend_key;
    }

    void Layer::SetName(const std::string& name)
    {
        detail::SetLayerName(*record_, name);
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
        return detail::DecodeLayerPixels(*record_, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb());
    }

    void Layer::SetPixels(const ImageView& image)
    {
        detail::CheckLayerImage(image, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb());
        ImageView view = image;
        view.color_mode = document_->GetColorMode(); // an empty image says nothing about its mode

        // Transparency would make it an ordinary layer, so it stays as AddBackgroundLayer made it.
        if (IsBackground() && (image.IsEmpty() || image.width != document_->GetWidth() || image.height != document_->GetHeight() ||
                               detail::HasTransparency(view)))
            throw std::invalid_argument("ffpsd: the background takes an opaque image of the canvas size");
        detail::ReplaceLayerPixels(*record_, view, IsBackground(), document_->IsPsb());
        document_->SetHasRealMergedData(false);
    }

    void Layer::CheckTransformable(const char* what) const
    {
        if (GetKind() != LayerKind::kRaster)
            throw std::invalid_argument(std::string("ffpsd: ") + what + " needs a raster layer");
        if (detail::HasVectorMask(*record_))
            throw std::invalid_argument(std::string("ffpsd: ") + what + " of a layer with a vector mask is not supported yet");
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

        std::vector<std::uint8_t> mask_data = record_->mask_data;
        detail::ShiftMaskBounds(mask_data, std::int64_t{top} - bounds.top, std::int64_t{left} - bounds.left);

        record_->bounds = {top, left, static_cast<std::int32_t>(bottom), static_cast<std::int32_t>(right)};
        record_->mask_data = std::move(mask_data);
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

        const Rect bounds = record_->bounds;
        if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0)
            throw std::invalid_argument("ffpsd: an empty layer has nothing to resize");

        detail::ScaledLayer scaled = detail::ScaleLayer(
            *record_, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb(), false,
            static_cast<double>(height) / bounds.GetHeight(), static_cast<double>(width) / bounds.GetWidth(), bounds.top, bounds.left,
            filter);
        record_->bounds = scaled.bounds;
        record_->channels = std::move(scaled.channels);
        record_->mask_data = std::move(scaled.mask_data);
        document_->SetHasRealMergedData(false);
    }

    std::optional<LayerMask> Layer::GetMask() const
    {
        return detail::DecodeLayerMask(*record_, document_->GetDepth(), document_->IsPsb());
    }

    void Layer::SetMask(const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color)
    {
        if (IsBackground())
            throw std::invalid_argument("ffpsd: the background has no mask");
        if (detail::HasVectorMask(*record_))
            throw std::invalid_argument("ffpsd: the mask of a layer with a vector mask is not supported yet");
        if (default_color != 0 && default_color != 255)
            throw std::invalid_argument("ffpsd: a mask's default color is 0 or 255, not " + std::to_string(default_color));
        detail::CheckLayerSides(image.width, image.height, document_->IsPsb());
        if (!image.IsEmpty())
        {
            if (image.channel_count != 1 || image.color_mode != ColorMode::kGrayscale || image.depth != document_->GetDepth())
                throw std::invalid_argument(
                    "ffpsd: a mask is one gray plane of " + std::to_string(document_->GetDepth()) + " bit, not " +
                    std::to_string(image.channel_count) + " of " + std::to_string(image.depth) + " bit in color mode " +
                    std::to_string(static_cast<int>(image.color_mode)));
            detail::CheckImage(image);
        }

        detail::ReplaceLayerMask(*record_, image, top, left, default_color, document_->IsPsb());
        document_->SetHasRealMergedData(false);
    }

    bool Layer::RemoveMask()
    {
        if (detail::FindPixelMaskId(*record_) == 0)
            return false;
        if (detail::HasVectorMask(*record_))
            throw std::invalid_argument("ffpsd: the mask of a layer with a vector mask is not supported yet");

        std::vector<detail::ChannelImageData>& channels = record_->channels;
        channels.erase(
            std::remove_if(
                channels.begin(), channels.end(),
                [](const detail::ChannelImageData& channel) { return channel.id == detail::kLayerMaskId; }),
            channels.end());
        record_->mask_data.clear();
        document_->SetHasRealMergedData(false);
        return true;
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
