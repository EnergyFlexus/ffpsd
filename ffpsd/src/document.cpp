#include "detail/color.hpp"
#include "detail/color_mode_data.hpp"
#include "detail/composite.hpp"
#include "detail/file_header.hpp"
#include "detail/image_data.hpp"
#include "detail/image_resources/document_specific_ids_seed_number.hpp"
#include "detail/image_resources/image_resource.hpp"
#include "detail/image_resources/version_info.hpp"
#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/file.hpp"
#include "detail/layer_and_mask/adjustments/adjustment_layer.hpp"
#include "detail/layer_and_mask/adjustments/levels.hpp"
#include "detail/layer_and_mask/layer_and_mask_info.hpp"
#include "detail/layer_and_mask/layer_info.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/layer_and_mask/layer_record.hpp"
#include "detail/layer_and_mask/tagged_blocks/layer_id.hpp"
#include "detail/pixel_data.hpp"

#include <algorithm>
#include <deque>
#include <ffpsd/document.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ffpsd
{
    namespace
    {
        constexpr std::uint16_t kVersionPsd = 1;
        constexpr std::uint16_t kVersionPsb = 2;
        constexpr std::uint16_t kMaxChannels = 56;

        // Resource 1005 stores signed 16.16, which cannot hold 32768.
        constexpr double kMaxResolution = 32767.0;

        // Why Photoshop could not open the stack, or null; bottom to top, a group's end marker comes first.
        const char* FindStackProblem(const detail::Layers& layers)
        {
            std::size_t open = 0;
            for (std::size_t i = 0; i < layers.size(); ++i)
            {
                if (i != 0 && layers[i]->IsBackground())
                    return "leaves the background off the bottom";

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

        void CheckLayerIndex(std::size_t index, std::size_t count)
        {
            if (index >= count)
                throw std::out_of_range("ffpsd: layer index " + std::to_string(index) + " of " + std::to_string(count));
        }

        void CheckResolution(double dpi)
        {
            // Written as a negated comparison so that a NaN is rejected too.
            if (!(dpi > 0.0) || dpi > kMaxResolution)
                throw std::invalid_argument("ffpsd: resolution out of range: " + std::to_string(dpi));
        }
    } // namespace

    struct Document::Impl
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;
        std::uint16_t version = kVersionPsd;
        ColorMode color = ColorMode::kRgb;

        std::vector<std::uint8_t> color_mode_data;
        detail::ImageResources image_resources;

        detail::Layers layers;

        // The rest of section 4, kept to write it back.
        detail::LayerAndMaskInfo layer_and_mask;

        // Section 5 as stored; empty when the file has none.
        detail::PixelData image_data;
    };

    Document::Document()
        : impl_(std::make_unique<Impl>())
    {
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
        return impl_->width;
    }
    std::uint32_t Document::GetHeight() const noexcept
    {
        return impl_->height;
    }
    std::uint16_t Document::GetChannelCount() const noexcept
    {
        return impl_->channel_count;
    }
    std::uint16_t Document::GetDepth() const noexcept
    {
        return impl_->depth;
    }
    ColorMode Document::GetColorMode() const noexcept
    {
        return impl_->color;
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
        if (index >= impl_->image_resources.size())
            throw std::out_of_range(
                "ffpsd: image resource index " + std::to_string(index) + " of " + std::to_string(impl_->image_resources.size()));

        return impl_->image_resources[index].get();
    }
    const ImageResource* Document::GetImageResourceById(std::uint16_t id) const noexcept
    {
        return detail::FindImageResource(impl_->image_resources, id);
    }

    void Document::SetWidth(std::uint32_t width)
    {
        const std::uint32_t max_side = detail::MaxSide(IsPsb());
        if (width < 1 || width > max_side)
            throw std::invalid_argument("ffpsd: width out of range: " + std::to_string(width));
        impl_->width = width;
    }
    void Document::SetHeight(std::uint32_t height)
    {
        const std::uint32_t max_side = detail::MaxSide(IsPsb());
        if (height < 1 || height > max_side)
            throw std::invalid_argument("ffpsd: height out of range: " + std::to_string(height));
        impl_->height = height;
    }
    void Document::SetChannelCount(std::uint16_t channel_count)
    {
        if (channel_count < 1 || channel_count > kMaxChannels)
            throw std::invalid_argument("ffpsd: channel count out of range: " + std::to_string(channel_count));
        impl_->channel_count = channel_count;
    }
    void Document::SetDepth(std::uint16_t depth)
    {
        if (depth != 1 && depth != 8 && depth != 16 && depth != 32)
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(depth));
        impl_->depth = depth;
    }
    void Document::SetColorMode(ColorMode color) noexcept
    {
        impl_->color = color;
    }
    void Document::SetPsb(bool psb)
    {
        if (psb == IsPsb())
            return;

        // Only RLE depends on the format; all is converted first, so a failure changes nothing.
        const bool from_psb = IsPsb();
        std::vector<std::pair<detail::PixelData*, detail::PixelData>> converted;
        const auto convert = [&](detail::PixelData& data) {
            if (data.NeedsConversion(from_psb, psb, data.GetCompression()))
                converted.emplace_back(&data, data.Converted(from_psb, psb, data.GetCompression()));
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
        CheckResolution(value.horizontal);
        CheckResolution(value.vertical);

        detail::SetImageResource(impl_->image_resources, value);
    }

    void Document::SetImageResource(const ImageResource& resource)
    {
        detail::FindOrInsertImageResource(impl_->image_resources, resource.id) = resource;
    }

    bool Document::RemoveImageResource(std::uint16_t id)
    {
        auto& resources = impl_->image_resources;
        const auto at = std::find_if(
            resources.begin(), resources.end(), [id](const std::unique_ptr<ImageResource>& resource) { return resource->id == id; });
        if (at == resources.end())
            return false;

        resources.erase(at);
        return true;
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

    Layer* Document::AddLayer(const std::string& name, const Image& image, std::int32_t top, std::int32_t left)
    {
        if (!image.IsEmpty() && image.depth != impl_->depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.depth) + " bit image in a " + std::to_string(impl_->depth) + " bit document");

        return AddLayer(name, image.bytes.data(), image.bytes.size(), image.width, image.height, image.channel_count, top, left);
    }

    Layer* Document::AddLayer(
        const std::string& name, const std::uint8_t* data, std::size_t size, std::uint32_t width, std::uint32_t height,
        std::uint16_t channel_count, std::int32_t top, std::int32_t left)
    {
        const std::size_t color_count = detail::LayerColorCount(impl_->color);

        // Keeps the size arithmetic below far from overflow.
        detail::CheckLayerSides(width, height, IsPsb());

        detail::SamplesView samples;
        samples.data = data;
        samples.size = size;
        samples.width = width;
        samples.height = height;
        samples.channel_count = channel_count;
        samples.depth = impl_->depth;

        auto record = std::make_unique<detail::LayerRecord>(detail::CreateLayerRecord(name, samples, top, left, color_count, IsPsb()));
        AssignLayerId(*record);
        impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        MarkStackChanged();
        return impl_->layers.back().get();
    }

    Layer* Document::AddBackgroundLayer(const std::string& name, const Image& image)
    {
        const std::size_t color_count = detail::LayerColorCount(impl_->color);
        if (image.depth != impl_->depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.depth) + " bit image in a " + std::to_string(impl_->depth) + " bit document");
        if (image.channel_count != color_count && image.channel_count != color_count + 1)
            throw std::invalid_argument(
                "ffpsd: the document needs " + std::to_string(color_count) + " color channels, the image has " +
                std::to_string(image.channel_count));
        if (image.width != impl_->width || image.height != impl_->height)
            throw std::invalid_argument(
                "ffpsd: a background is the document's size, " + std::to_string(impl_->width) + " x " + std::to_string(impl_->height) +
                ", not " + std::to_string(image.width) + " x " + std::to_string(image.height));
        if (!impl_->layers.empty() && impl_->layers.front()->IsBackground())
            throw std::logic_error("ffpsd: the document has a background already");

        Image flattened;
        if (image.channel_count > color_count)
        {
            flattened = detail::MakeWhiteImage(image.width, image.height, impl_->color, color_count, image.depth);
            detail::BlendNormal(flattened, image, 0, 0, 255, color_count);
        }
        const Image& opaque = flattened.IsEmpty() ? image : flattened;

        auto record =
            std::make_unique<detail::LayerRecord>(detail::CreateLayerRecord(name, detail::ViewOf(opaque), 0, 0, color_count, IsPsb()));
        detail::MarkAsBackground(*record);
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
            throw std::invalid_argument("ffpsd: only a raster layer can become the background");
        if (detail::HasLayerMask(*layer.record_))
            throw std::invalid_argument("ffpsd: a layer with a mask cannot become the background");

        const std::size_t color_count = detail::LayerColorCount(impl_->color);
        Image canvas = detail::MakeWhiteImage(impl_->width, impl_->height, impl_->color, color_count, impl_->depth);
        const Rect bounds = layer.GetBounds();
        detail::BlendNormal(canvas, layer.GetPixels(), bounds.top, bounds.left, layer.GetOpacity(), color_count);

        // Everything that can throw is done; from here on the layer only changes.
        std::vector<detail::ChannelImageData> channels = detail::EncodeLayerPixels(detail::ViewOf(canvas), color_count, IsPsb());
        detail::LayerRecord& record = *layer.record_;
        record.channels = std::move(channels);
        record.bounds = Rect{0, 0, static_cast<std::int32_t>(impl_->height), static_cast<std::int32_t>(impl_->width)};
        record.opacity = 255;
        record.blend_key = detail::Fourcc('n', 'o', 'r', 'm');
        detail::MarkAsBackground(record);

        const auto first = layers.begin();
        std::rotate(first, first + static_cast<std::ptrdiff_t>(index), first + static_cast<std::ptrdiff_t>(index) + 1);
        MarkStackChanged();
    }

    bool Document::UnsetBackgroundLayer()
    {
        if (impl_->layers.empty() || !impl_->layers.front()->IsBackground())
            return false;

        detail::UnmarkBackground(*impl_->layers.front()->record_);
        return true;
    }

    template <class T> Layer* Document::AddAdjustmentLayer(const std::string& name, const T& value)
    {
        const std::size_t color_count = detail::LayerColorCount(impl_->color);

        auto settings = std::make_unique<TaggedBlock>();
        settings->key = T::kKey;
        settings->data = detail::EncodeAdjustment(value);

        auto record = std::make_unique<detail::LayerRecord>(detail::CreateAdjustmentLayerRecord(name, std::move(settings), color_count));
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
        if (from.depth != impl_->depth || from.color != impl_->color || from.version != impl_->version)
            throw std::invalid_argument("ffpsd: the source layer comes from a document of another format");

        auto record = std::make_unique<detail::LayerRecord>(detail::CopyLayerRecord(*source.record_));
        if (detail::IsBackground(*record))
            detail::UnmarkBackground(*record);

        // The source's id stays with the source.
        AssignLayerId(*record);

        const bool was_valid = FindStackProblem(impl_->layers) == nullptr;
        impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::move(record), this)));
        if (const char* problem = was_valid ? FindStackProblem(impl_->layers) : nullptr)
        {
            impl_->layers.pop_back();
            throw std::invalid_argument(std::string("ffpsd: the copy ") + problem);
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
        return detail::DecodeImageData(impl_->image_data, impl_->width, impl_->height, impl_->channel_count, impl_->depth, IsPsb());
    }

    void Document::SetMergedImage(const Image& image)
    {
        if (image.width != impl_->width || image.height != impl_->height || image.channel_count != impl_->channel_count ||
            image.depth != impl_->depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.width) + " x " + std::to_string(image.height) + ", " +
                std::to_string(image.channel_count) + " channel, " + std::to_string(image.depth) + " bit merged image for a " +
                std::to_string(impl_->width) + " x " + std::to_string(impl_->height) + ", " + std::to_string(impl_->channel_count) +
                " channel, " + std::to_string(impl_->depth) + " bit document");

        impl_->image_data = detail::EncodeImageData(image, IsPsb());
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

    std::vector<std::uint8_t> Document::Save(Compression compression) const
    {
        if (impl_->width == 0 || impl_->height == 0 || impl_->channel_count == 0)
            throw std::logic_error("ffpsd: the document needs a size and channels before it is saved");

        detail::FileHeader header;
        header.version = impl_->version;
        header.channel_count = impl_->channel_count;
        header.height = impl_->height;
        header.width = impl_->width;
        header.depth = impl_->depth;
        header.color_mode = static_cast<std::uint16_t>(impl_->color);

        // Only data in another compression is packed again; a deque keeps the pointers to it valid.
        const auto target = static_cast<std::uint16_t>(compression);
        std::deque<detail::PixelData> repacked;
        const auto choose = [&](const detail::PixelData& data) {
            if (!data.NeedsConversion(IsPsb(), IsPsb(), target))
                return &data;
            return static_cast<const detail::PixelData*>(&repacked.emplace_back(data.Converted(IsPsb(), IsPsb(), target)));
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
            blank = detail::EncodeBlankImageData(impl_->width, impl_->height, impl_->channel_count, impl_->depth, IsPsb(), target);
        else
            composite = choose(impl_->image_data);

        detail::BigEndianWriter writer;
        detail::WriteFileHeader(writer, header);
        detail::WriteColorModeData(writer, impl_->color_mode_data);
        detail::WriteImageResources(writer, impl_->image_resources);
        detail::WriteLayerAndMaskInfo(writer, impl_->layer_and_mask, layers, IsPsb(), impl_->depth);
        writer.WriteU8Array(composite->GetBytes().data(), composite->GetBytes().size());
        return writer.Take();
    }

    void Document::Save(const std::string& path, Compression compression) const
    {
        detail::WriteFile(path, Save(compression));
    }

    Document Document::Open(const std::string& path)
    {
        return Parse(detail::ReadFile(path));
    }

    Document Document::Parse(const std::vector<std::uint8_t>& data)
    {
        return Parse(data.data(), data.size());
    }

    Document Document::Parse(const std::uint8_t* data, std::size_t size)
    {
        detail::BigEndianReader reader(data, size);
        const detail::FileHeader header = detail::ParseFileHeader(reader);

        Document doc;
        doc.SetPsb(header.version == kVersionPsb); // first: the size limits depend on it
        doc.SetChannelCount(header.channel_count);
        doc.SetHeight(header.height);
        doc.SetWidth(header.width);
        doc.SetDepth(header.depth);
        doc.SetColorMode(static_cast<ColorMode>(header.color_mode));

        doc.impl_->color_mode_data = detail::ParseColorModeData(reader);
        doc.impl_->image_resources = detail::ParseImageResources(reader);

        // A file that ends after section 3 has no layers; one cut inside section 4 throws.
        if (reader.AtEnd())
            return doc;

        std::vector<detail::LayerRecord> records;
        doc.impl_->layer_and_mask = detail::ParseLayerAndMaskInfo(reader, doc.IsPsb(), doc.GetDepth(), records);

        doc.impl_->layers.reserve(records.size());
        for (detail::LayerRecord& record : records)
            doc.impl_->layers.push_back(std::unique_ptr<Layer>(new Layer(std::make_unique<detail::LayerRecord>(std::move(record)), &doc)));

        doc.impl_->image_data = detail::ParseImageData(reader, header.width, header.height, header.channel_count, header.depth);
        return doc;
    }
} // namespace ffpsd
