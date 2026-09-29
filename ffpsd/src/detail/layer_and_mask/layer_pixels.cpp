#include "detail/layer_and_mask/layer_pixels.hpp"

#include "detail/file_header.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"
#include "detail/pixel_data.hpp"

#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
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

    SamplesView ViewOf(const Image& image) noexcept
    {
        SamplesView view;
        view.data = image.bytes.data();
        view.size = image.bytes.size();
        view.width = image.width;
        view.height = image.height;
        view.channel_count = image.channel_count;
        view.depth = image.depth;
        return view;
    }

    bool HasLayerMask(const LayerRecord& record) noexcept
    {
        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                return true;
        }
        return false;
    }

    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width > max_side || height > max_side)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " layer exceeds " + std::to_string(max_side) +
                " pixels a side");
    }

    std::vector<ChannelImageData> EncodeLayerPixels(const SamplesView& samples, std::size_t color_count, bool is_psb)
    {
        std::vector<ChannelImageData> channels;
        if (samples.width == 0 || samples.height == 0 || samples.channel_count == 0)
        {
            // Photoshop gives an empty layer every channel, each just a compression field.
            channels.push_back(EncodeChannelImageData(kTransparencyId, nullptr, 0, 0, 1, is_psb));
            for (std::size_t i = 0; i < color_count; ++i)
                channels.push_back(EncodeChannelImageData(static_cast<std::int16_t>(i), nullptr, 0, 0, 1, is_psb));
            return channels;
        }

        if (samples.depth != 8 && samples.depth != 16 && samples.depth != 32)
            throw std::invalid_argument("ffpsd: unsupported depth: " + std::to_string(samples.depth));
        if (samples.channel_count != color_count && samples.channel_count != color_count + 1)
            throw std::invalid_argument(
                "ffpsd: the document needs " + std::to_string(color_count) + " color channels, the image has " +
                std::to_string(samples.channel_count));

        const std::size_t plane_samples = std::size_t{samples.width} * samples.height;
        const std::size_t sample_size = samples.depth / 8u;
        const std::size_t needed = plane_samples * samples.channel_count * sample_size;
        if (samples.data == nullptr || samples.size != needed)
            throw std::invalid_argument(
                "ffpsd: samples hold " + std::to_string(samples.size) + " bytes, the geometry needs " + std::to_string(needed));

        const auto plane = [&](std::size_t index) { return samples.data + index * plane_samples * sample_size; };

        // Transparency is declared first, as Photoshop writes it.
        if (samples.channel_count > color_count)
            channels.push_back(
                EncodeChannelImageData(kTransparencyId, plane(color_count), samples.width, samples.height, sample_size, is_psb));
        for (std::size_t i = 0; i < color_count; ++i)
            channels.push_back(
                EncodeChannelImageData(static_cast<std::int16_t>(i), plane(i), samples.width, samples.height, sample_size, is_psb));
        return channels;
    }

    Rect BoundsAt(const SamplesView& samples, std::int32_t top, std::int32_t left)
    {
        if (samples.width == 0 || samples.height == 0 || samples.channel_count == 0)
            return {top, left, top, left};

        const std::int64_t bottom = std::int64_t{top} + samples.height;
        const std::int64_t right = std::int64_t{left} + samples.width;
        if (bottom > std::numeric_limits<std::int32_t>::max() || right > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ffpsd: layer bounds do not fit in 32 bits");
        return {top, left, static_cast<std::int32_t>(bottom), static_cast<std::int32_t>(right)};
    }

    Image DecodeLayerPixels(const LayerRecord& record, std::size_t color_count, std::uint16_t depth, bool is_psb)
    {
        if (depth != 8 && depth != 16 && depth != 32)
            throw std::runtime_error("ffpsd: " + std::to_string(depth) + " bit layer pixels are not supported");

        Image image;
        image.depth = depth;
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

        SwapSampleBytes(image.bytes, image.GetBytesPerSample());
        return image;
    }

    void ReplaceLayerPixels(LayerRecord& record, const SamplesView& samples, std::size_t color_count, bool is_psb)
    {
        std::vector<ChannelImageData> channels = EncodeLayerPixels(samples, color_count, is_psb);
        const Rect bounds = BoundsAt(samples, record.bounds.top, record.bounds.left);

        // Masks have their own rectangle, so new pixels leave them as they were.
        for (ChannelImageData& channel : record.channels)
        {
            if (channel.id < kTransparencyId)
                channels.push_back(std::move(channel));
        }
        record.channels = std::move(channels);
        record.bounds = bounds;
    }

    LayerRecord CreateLayerRecord(
        const std::string& name, const SamplesView& samples, std::int32_t top, std::int32_t left, std::size_t color_count, bool is_psb)
    {
        LayerRecord record;
        record.name = name;
        record.channels = EncodeLayerPixels(samples, color_count, is_psb);
        record.bounds = BoundsAt(samples, top, left);
        record.flags = kFlagsPhotoshop5;
        record.blending_ranges = DefaultBlendingRanges();
        SetTaggedBlock(record.blocks, UnicodeLayerName{name});
        return record;
    }
} // namespace ffpsd::detail
