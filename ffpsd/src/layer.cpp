#include "detail/image.hpp"
#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"
#include "detail/layer_and_mask/adjustments/levels.hpp"
#include "detail/layer_and_mask/layer_geometry.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"

#include <cstdint>
#include <ffpsd/document.hpp>
#include <ffpsd/layer.hpp>
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
            throw std::logic_error("ffpsd: SetAdjustment needs an adjustment layer of the same kind");

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
    ImageInfo Layer::GetPixelsInfo() const
    {
        return detail::LayerPixelsInfo(*record_, document_->GetColorMode(), document_->GetDepth());
    }
    void Layer::GetPixelsBytes(std::uint8_t* out, std::size_t size) const
    {
        detail::DecodeLayerPixels(*record_, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb(), out, size);
    }

    void Layer::SetPixels(const ImageView& image)
    {
        const ImageView view = detail::CheckLayerImage(image, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb());

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
            throw std::logic_error(std::string("ffpsd: ") + what + " needs a raster layer");
        detail::CheckCanvasBlocks(*record_, false);
    }

    void Layer::SetPosition(std::int32_t top, std::int32_t left)
    {
        const Rect bounds = record_->bounds;
        if (bounds.top == top && bounds.left == left)
            return;
        CheckTransformable("SetPosition");
        if (IsBackground())
            throw std::logic_error("ffpsd: the background stays at 0, 0");

        Apply(detail::Transform::Shift(static_cast<double>(left) - bounds.left, static_cast<double>(top) - bounds.top));
    }

    void Layer::Resize(std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        CheckTransformable("Resize");
        if (IsBackground())
            throw std::logic_error("ffpsd: the background keeps the canvas size");
        if (width == 0 || height == 0)
            throw std::invalid_argument("ffpsd: cannot resize a layer to " + std::to_string(width) + " x " + std::to_string(height));
        detail::CheckLayerSides(width, height, document_->IsPsb());

        const Rect bounds = record_->bounds;
        if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0)
            throw std::logic_error("ffpsd: an empty layer has nothing to resize");

        Apply(
            detail::Transform::Scale(
                static_cast<double>(width) / static_cast<double>(bounds.GetWidth()),
                static_cast<double>(height) / static_cast<double>(bounds.GetHeight()), bounds.left, bounds.top),
            filter);
    }

    void Layer::Flip(FlipDirection direction)
    {
        CheckTransformable("Flip");
        if (IsBackground())
            throw std::logic_error("ffpsd: the background turns only with the canvas");

        Apply(detail::Transform::Flip(direction, record_->bounds));
    }

    void Layer::Rotate(Rotation rotation)
    {
        CheckTransformable("Rotate");
        if (IsBackground())
            throw std::logic_error("ffpsd: the background turns only with the canvas");

        const Rect bounds = record_->bounds;
        detail::Transform transform = detail::Transform::Rotate(rotation, bounds);
        if (transform.SwapsAxes())
        {
            // Back to the old center, which falls between pixels when the sides differ by an odd number.
            const auto half_down = [](std::int64_t value) { return value >= 0 ? value / 2 : -((-value + 1) / 2); };
            const std::int64_t width = bounds.GetWidth();
            const std::int64_t height = bounds.GetHeight();
            transform = transform.Then(
                detail::Transform::Shift(static_cast<double>(half_down(width - height)), static_cast<double>(half_down(height - width))));
        }
        Apply(transform);
    }

    void Layer::Apply(const detail::Transform& transform, ResampleFilter filter)
    {
        detail::ApplyTransformed(
            *record_,
            detail::TransformLayer(*record_, document_->GetColorMode(), document_->GetDepth(), document_->IsPsb(), transform, filter));
        document_->SetHasRealMergedData(false);
    }

    std::optional<LayerMask> Layer::GetMask() const
    {
        return detail::DecodeLayerMask(*record_, document_->GetDepth(), document_->IsPsb());
    }

    void Layer::SetMask(const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color)
    {
        if (IsBackground())
            throw std::logic_error("ffpsd: the background has no mask");
        if (detail::HasVectorMask(*record_))
            throw std::logic_error("ffpsd: the mask of a layer with a vector mask is not supported yet");
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
            throw std::logic_error("ffpsd: the mask of a layer with a vector mask is not supported yet");

        detail::RemoveChannels(*record_, detail::kLayerMaskId);
        record_->mask_data.clear();
        document_->SetHasRealMergedData(false);
        return true;
    }

    std::size_t Layer::GetChannelCount() const noexcept
    {
        return record_->channels.size();
    }
    ChannelInfo Layer::GetChannelByIndex(std::size_t index) const
    {
        if (index >= record_->channels.size())
            throw std::out_of_range("ffpsd: channel index " + std::to_string(index) + " of " + std::to_string(record_->channels.size()));

        const detail::ChannelImageData& channel = record_->channels[index];
        ChannelInfo info;
        info.id = channel.id;
        info.compression = static_cast<ChannelCompression>(channel.data.GetCompression());
        info.size = channel.data.GetBytes().size();
        return info;
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
