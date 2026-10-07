#include "detail/layer_and_mask/layer_pixels.hpp"

#include "detail/color.hpp"
#include "detail/file_header.hpp"
#include "detail/image.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/pixel_data.hpp"

#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kVectorMaskKeys[] = {Fourcc('v', 'm', 's', 'k'), Fourcc('v', 's', 'm', 's')};

        // Bit 3, written by Photoshop 5.0 and later: Photoshop sets it on every layer.
        constexpr std::uint8_t kFlagsPhotoshop5 = 0x08;

        // Black low, black high, white low, white high, twice: source, then destination.
        constexpr std::uint8_t kPassEverything[] = {0, 0, 255, 255, 0, 0, 255, 255};

        // Photoshop writes five pairs whatever the color mode and the channel count.
        constexpr std::size_t kBlendingRangePairs = 5;

        std::vector<std::uint8_t> DefaultBlendingRanges()
        {
            std::vector<std::uint8_t> ranges;
            ranges.reserve(kBlendingRangePairs * sizeof(kPassEverything));
            for (std::size_t i = 0; i < kBlendingRangePairs; ++i)
                ranges.insert(ranges.end(), std::begin(kPassEverything), std::end(kPassEverything));
            return ranges;
        }

        // The color planes by channel id, then transparency; empty for a layer without area.
        struct LayerPlanes
        {
            ImageInfo info;
            std::vector<const ChannelImageData*> channels;
        };

        LayerPlanes FindLayerPlanes(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth)
        {
            if (!IsSampleDepth(depth))
                throw std::runtime_error(UnsupportedDepth(depth));

            LayerPlanes planes;
            planes.info.depth = depth;
            planes.info.color_mode = color_mode;
            const std::int64_t width = record.bounds.GetWidth();
            const std::int64_t height = record.bounds.GetHeight();
            if (width <= 0 || height <= 0)
                return planes;

            const std::size_t color_count = ColorChannelCount(color_mode);
            const ChannelImageData* transparency = nullptr;
            planes.channels.assign(color_count, nullptr);
            for (const ChannelImageData& channel : record.channels)
            {
                if (channel.id == kTransparencyId)
                    transparency = &channel;
                else if (channel.id >= 0 && static_cast<std::size_t>(channel.id) < color_count)
                    planes.channels[static_cast<std::size_t>(channel.id)] = &channel;
            }
            for (std::size_t i = 0; i < color_count; ++i)
            {
                if (planes.channels[i] == nullptr)
                    throw std::runtime_error("ffpsd: the layer has no channel " + std::to_string(i));
            }
            if (transparency != nullptr)
                planes.channels.push_back(transparency);

            planes.info.width = static_cast<std::uint32_t>(width);
            planes.info.height = static_cast<std::uint32_t>(height);
            planes.info.channel_count = static_cast<std::uint16_t>(planes.channels.size());
            return planes;
        }
    } // namespace

    bool HasLayerMask(const LayerRecord& record) noexcept
    {
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                return true;
        }
        return false;
    }

    bool HasVectorMask(const LayerRecord& record) noexcept
    {
        for (const std::uint32_t key : kVectorMaskKeys)
        {
            if (FindTaggedBlock(record.blocks, key) != nullptr)
                return true;
        }
        return false;
    }

    std::int16_t FindPixelMaskId(const LayerRecord& record) noexcept
    {
        bool has_layer_mask = false;
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id == kRealMaskId)
                return kRealMaskId;
            has_layer_mask = has_layer_mask || channel.id == kLayerMaskId;
        }
        return has_layer_mask && !IsRenderedMask(record.mask_data) ? kLayerMaskId : 0;
    }

    Image DecodeMask(const ChannelImageData& channel, const Rect& bounds, std::uint16_t depth, bool is_psb)
    {
        if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0)
            return MakeImage(0, 0, 1, depth, ColorMode::kGrayscale);

        Image mask = ReserveImage(
            static_cast<std::uint32_t>(bounds.GetWidth()), static_cast<std::uint32_t>(bounds.GetHeight()), 1, depth, ColorMode::kGrayscale);
        channel.data.Decode(is_psb, mask.bytes);
        return mask;
    }

    std::optional<LayerMask> DecodeLayerMask(const LayerRecord& record, std::uint16_t depth, bool is_psb)
    {
        const std::int16_t id = FindPixelMaskId(record);
        if (id == 0)
            return std::nullopt;

        LayerMask mask;
        mask.bounds = RequireMaskBounds(record.mask_data, id);
        mask.default_color = FindMaskDefaultColor(record.mask_data, id).value_or(0);
        mask.image = DecodeMask(*FindChannel(record, id), mask.bounds, depth, is_psb);
        return mask;
    }

    void ReplaceLayerMask(
        LayerRecord& record, const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color, bool is_psb)
    {
        const Rect bounds = BoundsAt(image, top, left);
        std::vector<std::uint8_t> mask_data = record.mask_data.empty() ? NewMaskData() : record.mask_data;
        SetMaskBounds(mask_data, kLayerMaskId, bounds);
        SetMaskDefaultColor(mask_data, kLayerMaskId, default_color);
        ChannelImageData channel = image.IsEmpty()
                                       ? EmptyChannel(kLayerMaskId)
                                       : EncodeChannelImageData(kLayerMaskId, image.data, image.width, image.height, image.depth, is_psb);
        RemoveChannels(record, kLayerMaskId);
        record.channels.push_back(std::move(channel));
        record.mask_data = std::move(mask_data);
    }

    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width > max_side || height > max_side)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " layer exceeds " + std::to_string(max_side) +
                " pixels a side");
    }

    ImageView CheckLayerImage(const ImageView& image, ColorMode color_mode, std::uint16_t depth, bool is_psb)
    {
        // First, so the size arithmetic below stays far from overflow.
        CheckLayerSides(image.width, image.height, is_psb);
        ImageView view = image;
        view.color_mode = color_mode;
        if (image.IsEmpty())
            return view;

        if (image.color_mode != color_mode)
            throw std::invalid_argument(
                "ffpsd: a color mode " + std::to_string(static_cast<int>(image.color_mode)) + " image in a color mode " +
                std::to_string(static_cast<int>(color_mode)) + " document");
        if (image.depth != depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.depth) + " bit image in a " + std::to_string(depth) + " bit document");
        CheckImage(image);
        CheckColorChannels(image);
        return view;
    }

    std::vector<ChannelImageData> EncodeLayerPixels(const ImageView& image, bool is_background, bool is_psb)
    {
        const std::size_t color_count = ColorChannelCount(image.color_mode);
        std::vector<ChannelImageData> channels;
        if (image.IsEmpty())
        {
            // Photoshop gives an empty layer every channel, each just a compression field.
            channels.push_back(EmptyChannel(kTransparencyId));
            for (std::size_t i = 0; i < color_count; ++i)
                channels.push_back(EmptyChannel(static_cast<std::int16_t>(i)));
            return channels;
        }

        const std::size_t plane_samples = std::size_t{image.width} * image.height;
        const std::size_t sample_size = image.depth / 8u;
        const auto plane = [&](std::size_t index) { return image.data + index * plane_samples * sample_size; };

        // Transparency is declared first, as Photoshop writes it; without it the bottom layer would become the background.
        if (image.channel_count > color_count)
            channels.push_back(EncodeChannelImageData(kTransparencyId, plane(color_count), image.width, image.height, image.depth, is_psb));
        else if (!is_background)
            channels.push_back(EncodeOpaqueChannel(kTransparencyId, image.width, image.height, image.depth, is_psb));
        for (std::size_t i = 0; i < color_count; ++i)
            channels.push_back(
                EncodeChannelImageData(static_cast<std::int16_t>(i), plane(i), image.width, image.height, image.depth, is_psb));
        return channels;
    }

    Rect BoundsAt(const ImageView& image, std::int32_t top, std::int32_t left)
    {
        if (image.IsEmpty())
            return {top, left, top, left};
        return MakeRect(top, left, std::int64_t{top} + image.height, std::int64_t{left} + image.width);
    }

    ImageInfo LayerPixelsInfo(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth)
    {
        return FindLayerPlanes(record, color_mode, depth).info;
    }

    Image DecodeLayerPixels(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb)
    {
        const LayerPlanes planes = FindLayerPlanes(record, color_mode, depth);
        const ImageInfo& info = planes.info;
        Image image = ReserveImage(info.width, info.height, info.channel_count, info.depth, info.color_mode);
        for (const ChannelImageData* channel : planes.channels)
            channel->data.Decode(is_psb, image.bytes);
        return image;
    }

    void DecodeLayerPixels(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, std::uint8_t* out, std::size_t size)
    {
        const LayerPlanes planes = FindLayerPlanes(record, color_mode, depth);
        CheckBytesSize(planes.info, out, size);
        const std::size_t plane = std::size_t{planes.info.width} * planes.info.height * planes.info.GetBytesPerSample();
        for (std::size_t i = 0; i < planes.channels.size(); ++i)
            planes.channels[i]->data.Decode(is_psb, out + i * plane);
    }

    void ReplaceLayerPixels(LayerRecord& record, const ImageView& image, bool is_background, bool is_psb)
    {
        std::vector<ChannelImageData> channels = EncodeLayerPixels(image, is_background, is_psb);
        const Rect bounds = BoundsAt(image, record.bounds.top, record.bounds.left);

        // Masks have their own rectangle, so new pixels leave them as they were.
        for (ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                channels.push_back(std::move(channel));
        }
        record.channels = std::move(channels);
        record.bounds = bounds;
    }

    LayerRecord
    CreateLayerRecord(const std::string& name, const ImageView& image, std::int32_t top, std::int32_t left, bool is_background, bool is_psb)
    {
        LayerRecord record;
        record.channels = EncodeLayerPixels(image, is_background, is_psb);
        record.bounds = BoundsAt(image, top, left);
        record.flags = kFlagsPhotoshop5;
        record.blending_ranges = DefaultBlendingRanges();
        SetLayerName(record, name);
        return record;
    }
} // namespace ffpsd::detail
