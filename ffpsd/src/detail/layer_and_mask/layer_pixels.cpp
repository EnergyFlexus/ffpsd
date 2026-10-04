#include "detail/layer_and_mask/layer_pixels.hpp"

#include "detail/color.hpp"
#include "detail/file_header.hpp"
#include "detail/image.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/pixel_data.hpp"

#include <iterator>
#include <limits>
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
        Image mask;
        mask.channel_count = 1;
        mask.depth = depth;
        mask.color_mode = ColorMode::kGrayscale;
        if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0)
            return mask;

        mask.width = static_cast<std::uint32_t>(bounds.GetWidth());
        mask.height = static_cast<std::uint32_t>(bounds.GetHeight());
        mask.bytes.resize(mask.GetSizeBytes());
        channel.data.Decode(is_psb, mask.bytes.data());
        return mask;
    }

    std::optional<LayerMask> DecodeLayerMask(const LayerRecord& record, std::uint16_t depth, bool is_psb)
    {
        const std::int16_t id = FindPixelMaskId(record);
        if (id == 0)
            return std::nullopt;

        const std::optional<Rect> bounds = FindMaskBounds(record.mask_data, id);
        if (!bounds.has_value())
            throw std::runtime_error("ffpsd: mask channel " + std::to_string(id) + " has no rectangle");

        LayerMask mask;
        mask.bounds = *bounds;
        mask.default_color = FindMaskDefaultColor(record.mask_data, id).value_or(0);
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id == id)
                mask.image = DecodeMask(channel, mask.bounds, depth, is_psb);
        }
        return mask;
    }

    void ReplaceLayerMask(
        LayerRecord& record, const ImageView& image, std::int32_t top, std::int32_t left, std::uint8_t default_color, bool is_psb)
    {
        const Rect bounds = BoundsAt(image, top, left);
        std::vector<std::uint8_t> mask_data = record.mask_data.empty() ? NewMaskData() : record.mask_data;
        SetMaskBounds(mask_data, kLayerMaskId, bounds);
        SetMaskDefaultColor(mask_data, kLayerMaskId, default_color);
        ChannelImageData channel = EncodeChannelImageData(
            kLayerMaskId, image.data, image.width, image.height, image.IsEmpty() ? 1 : image.GetBytesPerSample(), is_psb);

        std::vector<ChannelImageData> channels;
        for (ChannelImageData& kept : record.channels)
        {
            if (kept.id != kLayerMaskId)
                channels.push_back(std::move(kept));
        }
        channels.push_back(std::move(channel));
        record.channels = std::move(channels);
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

    void CheckLayerImage(const ImageView& image, ColorMode color_mode, std::uint16_t depth, bool is_psb)
    {
        // First, so the size arithmetic below stays far from overflow.
        CheckLayerSides(image.width, image.height, is_psb);
        if (image.IsEmpty())
            return;

        if (image.color_mode != color_mode)
            throw std::invalid_argument(
                "ffpsd: a color mode " + std::to_string(static_cast<int>(image.color_mode)) + " image in a color mode " +
                std::to_string(static_cast<int>(color_mode)) + " document");
        if (image.depth != depth)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(image.depth) + " bit image in a " + std::to_string(depth) + " bit document");
        CheckImage(image);
        CheckColorChannels(image);
    }

    std::vector<ChannelImageData> EncodeLayerPixels(const ImageView& image, bool is_background, bool is_psb)
    {
        const std::size_t color_count = ColorChannelCount(image.color_mode);
        std::vector<ChannelImageData> channels;
        if (image.IsEmpty())
        {
            // Photoshop gives an empty layer every channel, each just a compression field.
            channels.push_back(EncodeChannelImageData(kTransparencyId, nullptr, 0, 0, 1, is_psb));
            for (std::size_t i = 0; i < color_count; ++i)
                channels.push_back(EncodeChannelImageData(static_cast<std::int16_t>(i), nullptr, 0, 0, 1, is_psb));
            return channels;
        }

        const std::size_t plane_samples = std::size_t{image.width} * image.height;
        const std::size_t sample_size = image.depth / 8u;
        const auto plane = [&](std::size_t index) { return image.data + index * plane_samples * sample_size; };

        // Transparency is declared first, as Photoshop writes it; without it the bottom layer would become the background.
        if (image.channel_count > color_count)
            channels.push_back(EncodeChannelImageData(kTransparencyId, plane(color_count), image.width, image.height, sample_size, is_psb));
        else if (!is_background)
            channels.push_back(EncodeOpaqueChannel(kTransparencyId, image.width, image.height, image.depth, is_psb));
        for (std::size_t i = 0; i < color_count; ++i)
            channels.push_back(
                EncodeChannelImageData(static_cast<std::int16_t>(i), plane(i), image.width, image.height, sample_size, is_psb));
        return channels;
    }

    Rect BoundsAt(const ImageView& image, std::int32_t top, std::int32_t left)
    {
        if (image.IsEmpty())
            return {top, left, top, left};

        const std::int64_t bottom = std::int64_t{top} + image.height;
        const std::int64_t right = std::int64_t{left} + image.width;
        if (bottom > std::numeric_limits<std::int32_t>::max() || right > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
        return {top, left, static_cast<std::int32_t>(bottom), static_cast<std::int32_t>(right)};
    }

    Image DecodeLayerPixels(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb)
    {
        if (depth != 8 && depth != 16 && depth != 32)
            throw std::runtime_error("ffpsd: " + std::to_string(depth) + " bit layer pixels are not supported");

        const std::size_t color_count = ColorChannelCount(color_mode);
        Image image;
        image.depth = depth;
        image.color_mode = color_mode;
        const std::int64_t width = std::int64_t{record.bounds.right} - record.bounds.left;
        const std::int64_t height = std::int64_t{record.bounds.bottom} - record.bounds.top;
        if (width <= 0 || height <= 0)
            return image;

        const ChannelImageData* transparency = nullptr;
        std::vector<const ChannelImageData*> colors(color_count, nullptr);
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id == kTransparencyId)
                transparency = &channel;
            else if (channel.id >= 0 && static_cast<std::size_t>(channel.id) < color_count)
                colors[static_cast<std::size_t>(channel.id)] = &channel;
        }
        for (std::size_t i = 0; i < color_count; ++i)
        {
            if (colors[i] == nullptr)
                throw std::runtime_error("ffpsd: the layer has no channel " + std::to_string(i));
        }
        if (transparency != nullptr)
            colors.push_back(transparency);

        image.width = static_cast<std::uint32_t>(width);
        image.height = static_cast<std::uint32_t>(height);
        image.channel_count = static_cast<std::uint16_t>(colors.size());
        image.bytes.resize(image.GetSizeBytes());

        const std::size_t plane = std::size_t{image.width} * image.height * image.GetBytesPerSample();
        for (std::size_t i = 0; i < colors.size(); ++i)
            colors[i]->data.Decode(is_psb, image.bytes.data() + i * plane);
        return image;
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
