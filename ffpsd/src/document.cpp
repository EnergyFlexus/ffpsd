#include "detail/color.hpp"
#include "detail/color_mode_data.hpp"
#include "detail/composite.hpp"
#include "detail/file_header.hpp"
#include "detail/image.hpp"
#include "detail/image_data.hpp"
#include "detail/image_resources/document_specific_ids_seed_number.hpp"
#include "detail/image_resources/image_resource.hpp"
#include "detail/image_resources/resolution_info.hpp"
#include "detail/image_resources/version_info.hpp"
#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/file.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"
#include "detail/layer_and_mask/adjustments/levels.hpp"
#include "detail/layer_and_mask/layer_and_mask_info.hpp"
#include "detail/layer_and_mask/layer_geometry.hpp"
#include "detail/layer_and_mask/layer_info.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/layer_id.hpp"
#include "detail/limits.hpp"
#include "detail/pixel_data.hpp"
#include "detail/rect.hpp"
#include "detail/resample.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <ffpsd/document.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ffpsd
{
    namespace
    {
        using detail::kMaxChannels;
        using detail::kVersionPsb;
        using detail::kVersionPsd;

        // One allocation per layer, so pointers survive inserts, removals and moves.
        using Layers = std::vector<std::unique_ptr<Layer>>;

        // Why Photoshop could not open the stack, or null; bottom to top, a group's end marker comes first.
        const char* FindStackProblem(const Layers& layers)
        {
            std::size_t open = 0;
            for (std::size_t i = 0; i < layers.size(); ++i)
            {
                switch (layers[i]->GetKind())
                {
                case LayerKind::kGroupEnd:
                    ++open;
                    break;
                case LayerKind::kGroupOpen:
                case LayerKind::kGroupClosed:
                    if (open == 0)
                        return "breaks the group nesting";
                    --open;
                    break;
                default:
                    break;
                }
            }
            return open == 0 ? nullptr : "breaks the group nesting";
        }

        bool IsGroupHeader(const Layer& layer) noexcept
        {
            return layer.GetKind() == LayerKind::kGroupOpen || layer.GetKind() == LayerKind::kGroupClosed;
        }

        // The layer alone, or a group header with everything down to its end marker; bottom to top.
        std::vector<const Layer*> LayersToCopy(const Layers& layers, const Layer& source)
        {
            if (!IsGroupHeader(source))
                return {&source};

            const auto header = std::find_if(
                layers.begin(), layers.end(), [&source](const std::unique_ptr<Layer>& layer) { return layer.get() == &source; });
            std::size_t open = 0;
            for (auto at = header; at != layers.begin();)
            {
                --at;
                if (IsGroupHeader(**at))
                {
                    ++open;
                }
                else if ((*at)->GetKind() == LayerKind::kGroupEnd)
                {
                    if (open == 0)
                    {
                        std::vector<const Layer*> group;
                        for (auto in = at; in != header + 1; ++in)
                            group.push_back(in->get());
                        return group;
                    }
                    --open;
                }
            }
            throw std::invalid_argument("ffpsd: group '" + source.GetName() + "' has no end marker");
        }

        // Photoshop keeps no layers in these modes.
        void CheckKeepsLayers(ColorMode color_mode)
        {
            if (color_mode == ColorMode::kBitmap || color_mode == ColorMode::kIndexed || color_mode == ColorMode::kMultichannel)
                throw std::logic_error("ffpsd: color mode " + std::to_string(static_cast<int>(color_mode)) + " has no layers");
        }

        // Where the old side starts in the new one: 0, 1 and 2 are start, middle and end.
        std::int64_t AnchorOffset(std::uint32_t from, std::uint32_t to, int position)
        {
            // Truncated, so the odd pixel goes to the end whether the side grows or shrinks, as in Photoshop.
            const std::int64_t change = std::int64_t{to} - from;
            return position == 0 ? 0 : position == 2 ? change : change / 2;
        }

        void CheckLayerIndex(std::size_t index, std::size_t count)
        {
            if (index >= count)
                throw std::out_of_range("ffpsd: layer index " + std::to_string(index) + " of " + std::to_string(count));
        }
    } // namespace

    struct Document::Impl
    {
        std::uint16_t version = kVersionPsd;

        std::vector<std::uint8_t> color_mode_data;
        detail::ImageResources image_resources;

        Layers layers;

        // The rest of section 4, kept to write it back.
        detail::LayerAndMaskInfo layer_and_mask;

        // Section 5 as stored; empty when the file has none.
        detail::PixelData image_data;
        ImageInfo image_data_info;
    };

    Document::Document(std::unique_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }
    Document::Document(std::uint32_t width, std::uint32_t height, ColorMode color_mode, std::uint16_t depth)
        : Document(std::make_unique<Impl>())
    {
        detail::CheckModeAndDepth(color_mode, depth);
        impl_->version = std::max(width, height) > detail::MaxSide(false) ? kVersionPsb : kVersionPsd;
        detail::CheckCanvasSides(width, height, IsPsb());

        impl_->image_data_info = ImageInfo{width, height, detail::ColorChannelCount(color_mode), depth, color_mode};
    }
    Document::~Document() = default;
    Document::Document(Document&& other) noexcept
        : impl_(std::move(other.impl_))
    {
        RebindLayers();
    }
    Document& Document::operator=(Document&& other) noexcept
    {
        impl_ = std::move(other.impl_);
        RebindLayers();
        return *this;
    }

    void Document::RebindLayers() noexcept
    {
        if (impl_ == nullptr)
            return;

        for (const std::unique_ptr<Layer>& layer : impl_->layers)
            layer->document_ = this;
    }

    // std::vector<Document> moves only while this holds, otherwise it copies.
    static_assert(std::is_nothrow_move_constructible_v<Document>);

    std::uint32_t Document::GetWidth() const noexcept
    {
        return impl_->image_data_info.width;
    }
    std::uint32_t Document::GetHeight() const noexcept
    {
        return impl_->image_data_info.height;
    }
    std::uint16_t Document::GetChannelCount() const noexcept
    {
        return impl_->image_data_info.channel_count;
    }
    std::uint16_t Document::GetDepth() const noexcept
    {
        return impl_->image_data_info.depth;
    }
    ColorMode Document::GetColorMode() const noexcept
    {
        return impl_->image_data_info.color_mode;
    }
    bool Document::IsPsb() const noexcept
    {
        return impl_->version == kVersionPsb;
    }
    bool Document::HasRealMergedData() const noexcept
    {
        return detail::HasRealMergedData(impl_->image_resources);
    }
    VersionInfo Document::GetVersionInfo() const
    {
        return detail::GetImageResource<VersionInfo>(impl_->image_resources).value_or(VersionInfo());
    }
    ResolutionInfo Document::GetResolutionInfo() const noexcept
    {
        return detail::GetImageResource<ResolutionInfo>(impl_->image_resources).value_or(ResolutionInfo());
    }

    std::size_t Document::GetImageResourceCount() const noexcept
    {
        return impl_->image_resources.size();
    }
    const ImageResource* Document::GetImageResourceByIndex(std::size_t index) const
    {
        return detail::ImageResourceAt(impl_->image_resources, index);
    }
    const ImageResource* Document::GetImageResourceById(std::uint16_t id) const noexcept
    {
        return detail::FindImageResource(impl_->image_resources, id);
    }

    void Document::ResizeCanvas(std::uint32_t width, std::uint32_t height, Anchor anchor)
    {
        detail::CheckCanvasSides(width, height, IsPsb());
        if (width == impl_->image_data_info.width && height == impl_->image_data_info.height)
            return;

        const auto position = static_cast<int>(anchor);
        const std::int64_t dx = AnchorOffset(impl_->image_data_info.width, width, position % 3);
        const std::int64_t dy = AnchorOffset(impl_->image_data_info.height, height, position / 3);
        Apply(detail::Transform::Shift(static_cast<double>(dx), static_cast<double>(dy)), width, height);
    }

    void Document::Resize(std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        detail::CheckCanvasSides(width, height, IsPsb());
        if (width == impl_->image_data_info.width && height == impl_->image_data_info.height)
            return;

        Apply(
            detail::Transform::Scale(
                static_cast<double>(width) / impl_->image_data_info.width, static_cast<double>(height) / impl_->image_data_info.height, 0,
                0),
            width, height, filter);
    }

    void Document::FlipCanvas(FlipDirection direction)
    {
        Apply(
            detail::Transform::Flip(direction, detail::CanvasRect(impl_->image_data_info.width, impl_->image_data_info.height)),
            impl_->image_data_info.width, impl_->image_data_info.height);
    }

    void Document::RotateCanvas(Rotation rotation)
    {
        const detail::Transform transform =
            detail::Transform::Rotate(rotation, detail::CanvasRect(impl_->image_data_info.width, impl_->image_data_info.height));
        if (transform.SwapsAxes())
            Apply(transform, impl_->image_data_info.height, impl_->image_data_info.width);
        else
            Apply(transform, impl_->image_data_info.width, impl_->image_data_info.height);
    }

    void Document::Apply(const detail::Transform& transform, std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        const Rect canvas = detail::CanvasRect(width, height);
        const Rect placed = transform.Apply(detail::CanvasRect(impl_->image_data_info.width, impl_->image_data_info.height));
        const bool fills_canvas = placed == canvas;
        const std::uint32_t old_width = transform.SwapsAxes() ? impl_->image_data_info.height : impl_->image_data_info.width;
        const bool resamples =
            placed.GetWidth() != static_cast<std::int64_t>(old_width) ||
            placed.GetHeight() !=
                static_cast<std::int64_t>(transform.SwapsAxes() ? impl_->image_data_info.width : impl_->image_data_info.height);
        const bool keeps_fractions = fills_canvas && !transform.SwapsAxes() && transform.a > 0 && transform.d > 0;
        const Layers& layers = impl_->layers;

        // Everything that can throw comes first, so a failure changes nothing.
        std::vector<detail::TransformedLayer> transformed;
        transformed.reserve(layers.size());
        for (const std::unique_ptr<Layer>& layer : layers)
        {
            detail::CheckCanvasBlocks(*layer->record_, keeps_fractions);
            transformed.push_back(
                detail::TransformLayer(
                    *layer->record_, impl_->image_data_info.color_mode, impl_->image_data_info.depth, IsPsb(), transform, filter,
                    layer->IsBackground() ? &canvas : nullptr));
        }

        detail::PixelData merged;
        if (!impl_->image_data.IsEmpty())
        {
            Image moved = detail::TransformPixels(
                GetMergedImage(), transform, static_cast<std::uint32_t>(placed.GetWidth()), static_cast<std::uint32_t>(placed.GetHeight()),
                filter, true);
            if (fills_canvas)
            {
                merged = detail::PixelData::Encode(moved, IsPsb());
            }
            else
            {
                Image blank = detail::MakeBlankComposite(width, height, moved);
                detail::PlaceImage(blank, moved, placed.top, placed.left);
                merged = detail::PixelData::Encode(blank, IsPsb());
            }
        }

        std::optional<ResolutionInfo> resolution;
        if (transform.SwapsAxes())
            resolution = detail::GetImageResource<ResolutionInfo>(impl_->image_resources);

        impl_->image_data_info.width = width;
        impl_->image_data_info.height = height;
        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            detail::ApplyTransformed(*layers[i]->record_, std::move(transformed[i]));
        }
        if (!impl_->image_data.IsEmpty())
            impl_->image_data = std::move(merged);
        if (resolution.has_value())
        {
            std::swap(resolution->horizontal, resolution->vertical);
            std::swap(resolution->horizontal_unit, resolution->vertical_unit);
            std::swap(resolution->width_unit, resolution->height_unit);
            detail::SetImageResource(impl_->image_resources, *resolution);
        }

        // Moved samples keep the composite real; resampled or cut ones leave it to the layers.
        if (!layers.empty() && (resamples || !fills_canvas))
            SetHasRealMergedData(false);
    }

    void Document::ConvertColorMode(ColorMode color_mode)
    {
        const ColorMode from = impl_->image_data_info.color_mode;
        if (color_mode == from)
            return;

        const bool to_gray = from == ColorMode::kRgb && color_mode == ColorMode::kGrayscale;
        if (!to_gray && !(from == ColorMode::kGrayscale && color_mode == ColorMode::kRgb))
            throw std::invalid_argument(
                "ffpsd: converting color mode " + std::to_string(static_cast<int>(from)) + " to " +
                std::to_string(static_cast<int>(color_mode)) + " is not supported");
        if (!to_gray)
            detail::CheckChannelCount(impl_->image_data_info.channel_count + 2u, "the document in RGB");

        Image (*const convert)(const ImageView&) = to_gray ? detail::RgbToGray : detail::GrayToRgb;
        const Layers& layers = impl_->layers;

        // Everything that can throw comes first, while the document keeps its mode.
        std::vector<Image> pixels(layers.size());
        std::vector<std::optional<TaggedBlock>> levels(layers.size());
        bool has_adjustments = false;
        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            const Layer& layer = *layers[i];
            const std::uint32_t key = layer.GetAdjustmentKey();
            if (key == LevelsInfo::kKey)
            {
                const LevelsInfo value = detail::ConvertLevels(*layer.GetAdjustment<LevelsInfo>(), to_gray);
                levels[i] = *layer.GetTaggedBlockByKey(key);
                levels[i]->data = detail::EncodeAdjustment(value);
                has_adjustments = true;
            }
            else if (key != 0)
            {
                throw std::invalid_argument(
                    "ffpsd: the '" + detail::FourccString(key) + "' adjustment cannot be converted to " + (to_gray ? "gray" : "RGB"));
            }

            // Groups and adjustments have no pixels, but their empty channels are converted too.
            const Image layer_pixels = layer.GetPixels();
            if (!layer_pixels.IsEmpty())
                pixels[i] = convert(layer_pixels);
        }

        // Levels are not linear: gray of an adjusted RGB composite is not what the gray layers give, but three equal channels are.
        const bool exact = HasRealMergedData() && (!to_gray || !has_adjustments);
        const Image merged = exact ? GetMergedImage() : Image();
        const Image converted = merged.IsEmpty() ? Image() : convert(merged);

        impl_->image_data_info.color_mode = color_mode;
        impl_->image_data_info.channel_count =
            static_cast<std::uint16_t>(to_gray ? impl_->image_data_info.channel_count - 2 : impl_->image_data_info.channel_count + 2);

        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            layers[i]->SetPixels(pixels[i]);
            if (levels[i].has_value())
                layers[i]->SetTaggedBlock(*levels[i]);
        }

        if (converted.IsEmpty())
            DropMergedImage();
        else
            SetMergedImage(converted);
        SetHasRealMergedData(!converted.IsEmpty());

        // A profile of one mode does not describe the other.
        RemoveImageResource(1039);
    }

    void Document::SetPsb(bool psb)
    {
        if (psb == IsPsb())
            return;

        // Only RLE depends on the format; all is converted first, so a failure changes nothing.
        const bool from_psb = IsPsb();
        std::vector<std::pair<detail::PixelData*, detail::PixelData>> converted;
        const auto convert = [&](detail::PixelData& data) {
            const Compression kept = data.GetCompression() == detail::kCompressionRaw ? Compression::kRaw : Compression::kRle;
            if (std::optional<detail::PixelData> result = data.Converted(from_psb, psb, kept))
                converted.emplace_back(&data, std::move(*result));
        };
        for (const std::unique_ptr<Layer>& layer : impl_->layers)
        {
            for (detail::ChannelImageData& channel : layer->record_->channels)
                convert(channel.data);
        }
        convert(impl_->image_data);

        for (auto& [target, data] : converted)
            *target = std::move(data);
        impl_->version = psb ? kVersionPsb : kVersionPsd;
    }

    // The block is the only storage, so getters also see resources written by hand.
    void Document::SetHasRealMergedData(bool value)
    {
        detail::SetHasRealMergedData(impl_->image_resources, value);
    }
    void Document::SetVersionInfo(VersionInfo value)
    {
        detail::SetImageResource(impl_->image_resources, value);
    }
    void Document::SetResolutionInfo(ResolutionInfo value)
    {
        // Both checked before either is stored.
        detail::CheckResolution(value.horizontal);
        detail::CheckResolution(value.vertical);

        detail::SetImageResource(impl_->image_resources, value);
    }

    void Document::SetImageResource(const ImageResource& resource)
    {
        detail::FindOrInsertImageResource(impl_->image_resources, resource.id) = resource;
    }

    bool Document::RemoveImageResource(std::uint16_t id)
    {
        return detail::RemoveImageResource(impl_->image_resources, id);
    }

    std::size_t Document::GetLayerCount() const noexcept
    {
        return impl_->layers.size();
    }
    Layer* Document::GetLayerByIndex(std::size_t index)
    {
        CheckLayerIndex(index, impl_->layers.size());
        return impl_->layers[index].get();
    }
    const Layer* Document::GetLayerByIndex(std::size_t index) const
    {
        CheckLayerIndex(index, impl_->layers.size());
        return impl_->layers[index].get();
    }

    std::optional<std::size_t> Document::FindLayerIndex(const Layer& layer) const noexcept
    {
        for (std::size_t i = 0; i < impl_->layers.size(); ++i)
        {
            if (impl_->layers[i].get() == &layer)
                return i;
        }
        return std::nullopt;
    }

    Layer* Document::AddLayer(const std::string& name, const ImageView& image, std::int32_t top, std::int32_t left)
    {
        CheckKeepsLayers(impl_->image_data_info.color_mode);
        detail::CheckLayerSides(image.width, image.height, IsPsb());
        detail::CheckImageFormat(image, impl_->image_data_info.color_mode, impl_->image_data_info.depth);

        auto record = std::make_unique<detail::LayerRecord>(detail::LayerRecord::Empty(name, top, left));
        detail::ReplaceLayerPixels(*record, image, impl_->image_data_info.color_mode, false, IsPsb());
        AssignLayerId(*record);
        impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        MarkStackChanged();
        return impl_->layers.back().get();
    }

    Layer* Document::AddGroup(const std::string& name)
    {
        CheckKeepsLayers(impl_->image_data_info.color_mode);

        const auto add = [this](detail::LayerRecord made) {
            auto record = std::make_unique<detail::LayerRecord>(std::move(made));
            detail::ReplaceLayerPixels(*record, ImageView(), impl_->image_data_info.color_mode, false, IsPsb());
            AssignLayerId(*record);
            impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
            return impl_->layers.back().get();
        };

        // Bottom to top, the end marker first; a whole group keeps the stack as valid as it was.
        add(detail::LayerRecord::GroupEnd());
        Layer* header = add(detail::LayerRecord::GroupHeader(name));
        MarkStackChanged();
        return header;
    }

    Layer* Document::AddBackgroundLayer(const std::string& name, const ImageView& image)
    {
        CheckKeepsLayers(impl_->image_data_info.color_mode);
        detail::CheckLayerSides(image.width, image.height, IsPsb());
        detail::CheckImageFormat(image, impl_->image_data_info.color_mode, impl_->image_data_info.depth);
        if (image.IsEmpty() || image.width != impl_->image_data_info.width || image.height != impl_->image_data_info.height)
            throw std::invalid_argument(
                "ffpsd: a background is the document's size, " + std::to_string(impl_->image_data_info.width) + " x " +
                std::to_string(impl_->image_data_info.height) + ", not " + std::to_string(image.width) + " x " +
                std::to_string(image.height));
        if (!impl_->layers.empty() && impl_->layers.front()->IsBackground())
            throw std::logic_error("ffpsd: the document has a background already");

        Image flattened;
        if (detail::HasTransparency(image))
        {
            flattened = detail::MakeWhiteImage(image.width, image.height, impl_->image_data_info.color_mode, image.depth);
            detail::BlendNormal(flattened, image, 0, 0, 255);
        }
        const ImageView opaque = flattened.IsEmpty() ? image : ImageView(flattened);

        auto record = std::make_unique<detail::LayerRecord>(detail::LayerRecord::Empty(name, 0, 0));
        detail::ReplaceLayerPixels(*record, opaque, impl_->image_data_info.color_mode, true, IsPsb());
        record->MarkAsBackground();
        AssignLayerId(*record);
        impl_->layers.insert(impl_->layers.begin(), std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        MarkStackChanged();
        return impl_->layers.front().get();
    }

    void Document::SetBackgroundLayer(std::size_t index)
    {
        auto& layers = impl_->layers;
        CheckLayerIndex(index, layers.size());
        Layer& layer = *layers[index];
        if (layer.IsBackground())
            return;
        if (layers.front()->IsBackground())
            throw std::logic_error("ffpsd: the document has a background already");
        if (layer.GetKind() != LayerKind::kRaster)
            throw std::logic_error("ffpsd: only a raster layer can become the background");
        if (layer.record_->HasLayerMask())
            throw std::logic_error("ffpsd: a layer with a mask cannot become the background");

        Image canvas = detail::MakeWhiteImage(
            impl_->image_data_info.width, impl_->image_data_info.height, impl_->image_data_info.color_mode, impl_->image_data_info.depth);
        const Rect bounds = layer.GetBounds();
        detail::BlendNormal(canvas, layer.GetPixels(), bounds.top, bounds.left, layer.GetOpacity());

        // Everything that can throw is done; from here on the layer only changes.
        std::vector<detail::ChannelImageData> channels =
            detail::EncodeLayerPixels(canvas, impl_->image_data_info.color_mode, true, IsPsb());
        detail::LayerRecord& record = *layer.record_;
        record.channels = std::move(channels);
        record.bounds = detail::CanvasRect(impl_->image_data_info.width, impl_->image_data_info.height);
        record.opacity = 255;
        record.blend_key = detail::Fourcc('n', 'o', 'r', 'm');
        record.MarkAsBackground();

        const auto first = layers.begin();
        std::rotate(first, first + static_cast<std::ptrdiff_t>(index), first + static_cast<std::ptrdiff_t>(index) + 1);
        MarkStackChanged();
    }

    bool Document::UnsetBackgroundLayer()
    {
        if (impl_->layers.empty() || !impl_->layers.front()->IsBackground())
            return false;

        impl_->layers.front()->record_->UnmarkBackground(impl_->image_data_info.depth, IsPsb());
        return true;
    }

    template <class T> Layer* Document::AddAdjustmentLayer(const std::string& name, const T& value)
    {
        CheckKeepsLayers(impl_->image_data_info.color_mode);

        auto settings = std::make_unique<TaggedBlock>();
        settings->key = T::kKey;
        settings->data = detail::EncodeAdjustment(value);

        auto record = std::make_unique<detail::LayerRecord>(
            detail::CreateAdjustmentLayerRecord(name, std::move(settings), impl_->image_data_info.color_mode));
        AssignLayerId(*record);
        impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        MarkStackChanged();
        return impl_->layers.back().get();
    }

    // A line per struct in adjustments.hpp, as in layer.cpp.
    template FFPSD_EXPORT Layer* Document::AddAdjustmentLayer<LevelsInfo>(const std::string& name, const LevelsInfo& value);

    Layer* Document::AddLayerCopy(const Layer& source)
    {
        const Impl& from = *source.document_->impl_;
        if (from.image_data_info.depth != impl_->image_data_info.depth ||
            from.image_data_info.color_mode != impl_->image_data_info.color_mode || from.version != impl_->version)
            throw std::invalid_argument("ffpsd: the source layer comes from a document of another format");
        if (source.GetKind() == LayerKind::kGroupEnd)
            throw std::invalid_argument("ffpsd: a group's end marker is copied with the group, through its header");

        // All copied before any is added, as the source may be in this document.
        std::vector<std::unique_ptr<detail::LayerRecord>> records;
        for (const Layer* layer : LayersToCopy(from.layers, source))
            records.push_back(std::make_unique<detail::LayerRecord>(layer->record_->Copy()));
        if (source.IsBackground())
            records.back()->UnmarkBackground(impl_->image_data_info.depth, IsPsb());

        // A whole group nests as it did, so the stack stays as valid as it was; the sources' ids stay with them.
        for (std::unique_ptr<detail::LayerRecord>& record : records)
        {
            AssignLayerId(*record);
            impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        }

        MarkStackChanged();
        return impl_->layers.back().get();
    }

    void Document::RemoveLayer(std::size_t index)
    {
        CheckLayerIndex(index, impl_->layers.size());

        auto& layers = impl_->layers;
        const bool was_valid = FindStackProblem(layers) == nullptr;
        std::unique_ptr<Layer> removed = std::move(layers[index]);
        layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(index));
        if (const char* problem = was_valid ? FindStackProblem(layers) : nullptr)
        {
            layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(index), std::move(removed));
            throw std::invalid_argument("ffpsd: removing layer " + std::to_string(index) + " " + problem);
        }

        MarkStackChanged();
    }

    void Document::MoveLayer(std::size_t from, std::size_t to)
    {
        auto& layers = impl_->layers;
        CheckLayerIndex(from, layers.size());
        CheckLayerIndex(to, layers.size());
        if (from == to)
            return;

        const auto rotate = [&](std::size_t a, std::size_t b) {
            const auto first = layers.begin();
            if (a < b)
                std::rotate(
                    first + static_cast<std::ptrdiff_t>(a), first + static_cast<std::ptrdiff_t>(a) + 1,
                    first + static_cast<std::ptrdiff_t>(b) + 1);
            else
                std::rotate(
                    first + static_cast<std::ptrdiff_t>(b), first + static_cast<std::ptrdiff_t>(a),
                    first + static_cast<std::ptrdiff_t>(a) + 1);
        };

        // Photoshop keeps the background at the bottom: it does not move, and nothing goes under it.
        if ((from == 0 || to == 0) && layers.front()->IsBackground())
            throw std::invalid_argument(
                "ffpsd: moving layer " + std::to_string(from) + " to " + std::to_string(to) + " leaves the background off the bottom");

        const bool was_valid = FindStackProblem(layers) == nullptr;
        rotate(from, to);
        if (const char* problem = was_valid ? FindStackProblem(layers) : nullptr)
        {
            rotate(to, from);
            throw std::invalid_argument("ffpsd: moving layer " + std::to_string(from) + " to " + std::to_string(to) + " " + problem);
        }

        MarkStackChanged();
    }

    std::size_t Document::GetTaggedBlockCount() const noexcept
    {
        return impl_->layer_and_mask.blocks.size();
    }
    const TaggedBlock* Document::GetTaggedBlockByIndex(std::size_t index) const
    {
        return detail::TaggedBlockAt(impl_->layer_and_mask.blocks, index);
    }
    const TaggedBlock* Document::GetTaggedBlockByKey(std::uint32_t key) const noexcept
    {
        return detail::FindTaggedBlock(impl_->layer_and_mask.blocks, key);
    }
    void Document::SetTaggedBlock(const TaggedBlock& block)
    {
        detail::FindOrAppendTaggedBlock(impl_->layer_and_mask.blocks, block.key) = block;
    }
    bool Document::RemoveTaggedBlock(std::uint32_t key)
    {
        return detail::RemoveTaggedBlock(impl_->layer_and_mask.blocks, key);
    }

    Image Document::GetMergedImage() const
    {
        return impl_->image_data.Decode(GetMergedImageInfo(), IsPsb());
    }
    ImageInfo Document::GetMergedImageInfo() const
    {
        return detail::ImageDataInfo(impl_->image_data, impl_->image_data_info);
    }
    void Document::GetMergedImageBytes(std::uint8_t* out, std::size_t size) const
    {
        impl_->image_data.Decode(GetMergedImageInfo(), IsPsb(), out, size);
    }

    std::optional<ChannelCompression> Document::GetMergedCompression() const noexcept
    {
        if (impl_->image_data.IsEmpty())
            return std::nullopt;
        return static_cast<ChannelCompression>(impl_->image_data.GetCompression());
    }

    void Document::SetMergedImage(const ImageView& image)
    {
        const std::uint16_t colors = impl_->image_data_info.color_mode == ColorMode::kMultichannel
                                         ? 1
                                         : detail::ColorChannelCount(impl_->image_data_info.color_mode);
        if (image.width != impl_->image_data_info.width || image.height != impl_->image_data_info.height || image.channel_count < colors ||
            image.channel_count > kMaxChannels || image.depth != impl_->image_data_info.depth ||
            image.color_mode != impl_->image_data_info.color_mode)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.width) + " x " + std::to_string(image.height) + ", " +
                std::to_string(image.channel_count) + " channel, " + std::to_string(image.depth) + " bit, color mode " +
                std::to_string(static_cast<int>(image.color_mode)) + " merged image for a " + std::to_string(impl_->image_data_info.width) +
                " x " + std::to_string(impl_->image_data_info.height) + ", " + std::to_string(impl_->image_data_info.depth) +
                " bit, color mode " + std::to_string(static_cast<int>(impl_->image_data_info.color_mode)) + " document with " +
                std::to_string(colors) + " to " + std::to_string(kMaxChannels) + " channels");

        impl_->image_data = detail::PixelData::Encode(image, IsPsb());
        impl_->image_data_info.channel_count = image.channel_count;

        // Merged alpha takes the first extra channel for transparency; without one it would take a color.
        if (image.channel_count == colors)
            impl_->layer_and_mask.merged_alpha = false;
        SetHasRealMergedData(true);
    }

    void Document::AssignLayerId(detail::LayerRecord& record)
    {
        using detail::DocumentSpecificIdsSeedNumber;
        const std::optional<DocumentSpecificIdsSeedNumber> seed =
            detail::GetImageResource<DocumentSpecificIdsSeedNumber>(impl_->image_resources);
        std::uint32_t last = seed.has_value() ? seed->value : 0;
        for (const std::unique_ptr<Layer>& layer : impl_->layers)
        {
            if (const std::optional<detail::LayerId> id = detail::GetTaggedBlock<detail::LayerId>(layer->record_->blocks))
                last = std::max(last, id->id);
        }

        detail::SetTaggedBlock(record.blocks, detail::LayerId{last + 1});
        detail::SetImageResource(impl_->image_resources, DocumentSpecificIdsSeedNumber{last + 1});
    }

    // 1024 holds a layer index and 1026 and 1072 one entry per layer; stale ones are worse than none.
    void Document::MarkStackChanged()
    {
        auto& resources = impl_->image_resources;
        resources.erase(
            std::remove_if(
                resources.begin(), resources.end(),
                [](const std::unique_ptr<ImageResource>& resource) {
                    return resource->id == detail::kLayerStateInformation || resource->id == detail::kLayersGroupInformation ||
                           resource->id == detail::kLayerGroupsEnabledId;
                }),
            resources.end());

        SetHasRealMergedData(false);
    }

    void Document::DropMergedImage()
    {
        if (impl_->image_data.IsEmpty())
            return;

        impl_->image_data = detail::PixelData();
        SetHasRealMergedData(false);
    }

    std::vector<std::uint8_t> Document::Save(Compression compression) const
    {
        // Only data in another compression is packed again; a deque keeps the pointers to it valid.
        std::deque<detail::PixelData> repacked;
        const auto choose = [&](const detail::PixelData& data) {
            std::optional<detail::PixelData> result = data.Converted(IsPsb(), IsPsb(), compression);
            if (!result.has_value())
                return &data;
            return static_cast<const detail::PixelData*>(&repacked.emplace_back(std::move(*result)));
        };

        std::vector<detail::LayerToWrite> layers;
        layers.reserve(impl_->layers.size());
        for (const std::unique_ptr<Layer>& layer : impl_->layers)
        {
            detail::LayerToWrite entry;
            entry.record = layer->record_.get();
            for (const detail::ChannelImageData& channel : entry.record->channels)
                entry.channels.push_back(choose(channel.data));
            layers.push_back(std::move(entry));
        }

        detail::PixelData blank;
        const detail::PixelData* composite = &blank;
        if (impl_->image_data.IsEmpty())
            blank = detail::PixelData::Blank(impl_->image_data_info, IsPsb(), compression);
        else
            composite = choose(impl_->image_data);

        detail::BigEndianWriter writer(composite->GetBytes().size() + detail::PixelBytes(layers) + detail::kWriteHeadroom);
        detail::WriteFileHeader(writer, detail::FileHeader{impl_->version, impl_->image_data_info});
        detail::WriteColorModeData(writer, impl_->color_mode_data);
        detail::WriteImageResources(writer, impl_->image_resources);
        detail::WriteLayerAndMaskInfo(writer, impl_->layer_and_mask, layers, IsPsb(), impl_->image_data_info.depth);
        detail::WriteImageData(writer, *composite);
        return writer.Take();
    }

    void Document::Save(const std::string& path, Compression compression) const
    {
        detail::WriteFile(path, Save(compression));
    }

    Document Document::Open(const std::string& path)
    {
        const Bytes file = detail::ReadFile(path);
        return Parse(file.data(), file.size());
    }

    Document Document::Parse(const std::vector<std::uint8_t>& data)
    {
        return Parse(data.data(), data.size());
    }

    Document Document::Parse(const std::uint8_t* data, std::size_t size)
    {
        detail::BigEndianReader reader(data, size);
        const detail::FileHeader header = detail::ParseFileHeader(reader);

        Document doc(std::make_unique<Impl>());
        doc.impl_->version = header.version;
        doc.impl_->image_data_info = header.info;

        doc.impl_->color_mode_data = detail::ParseColorModeData(reader);
        doc.impl_->image_resources = detail::ParseImageResources(reader);

        // A file that ends after section 3 has no layers; one cut inside section 4 throws.
        if (reader.AtEnd())
            return doc;

        detail::ParsedLayerAndMaskInfo parsed = detail::ParseLayerAndMaskInfo(reader, doc.IsPsb(), doc.GetDepth());
        doc.impl_->layer_and_mask = std::move(parsed.info);

        doc.impl_->layers.reserve(parsed.records.size());
        for (detail::LayerRecord& record : parsed.records)
            doc.impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::make_unique<detail::LayerRecord>(std::move(record)), &doc)));

        doc.impl_->image_data = detail::ParseImageData(reader, doc.impl_->image_data_info);
        return doc;
    }
} // namespace ffpsd
