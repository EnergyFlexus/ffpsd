#include "detail/layer_and_mask/layer_pixels.hpp"

#include "detail/color.hpp"
#include "detail/image.hpp"
#include "detail/layer_and_mask/channel_ids.hpp"
#include "detail/limits.hpp"
#include "detail/pixel_data.hpp"
#include "detail/rect.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        // The color planes by channel id, then transparency; empty for a layer without area.
        struct LayerPlanes
        {
            ImageInfo info;
            std::vector<const ChannelImageData*> channels;
        };

        LayerPlanes FindLayerPlanes(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth)
        {
            CheckSampleDepth<std::runtime_error>(depth);

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

    std::vector<ChannelImageData> EncodeLayerPixels(const ImageView& image, ColorMode color_mode, bool is_background, bool is_psb)
    {
        const std::size_t color_count = ColorChannelCount(color_mode);
        std::vector<ChannelImageData> channels;
        if (image.IsEmpty())
        {
            // Photoshop gives an empty layer every channel, each just a compression field.
            channels.push_back(ChannelImageData::Empty(kTransparencyId));
            for (std::size_t i = 0; i < color_count; ++i)
                channels.push_back(ChannelImageData::Empty(static_cast<std::int16_t>(i)));
            return channels;
        }

        const std::size_t plane_samples = std::size_t{image.width} * image.height;
        const std::size_t sample_size = image.GetBytesPerSample();
        const auto encode = [&](std::int16_t id, std::size_t plane) {
            const std::uint8_t* samples = image.data + plane * plane_samples * sample_size;
            return ChannelImageData::Encode(id, samples, image.width, image.height, image.depth, is_psb);
        };

        // Transparency is declared first, as Photoshop writes it; without it the bottom layer would become the background.
        if (HasTransparency(image))
            channels.push_back(encode(kTransparencyId, color_count));
        else if (!is_background)
            channels.push_back(ChannelImageData::Opaque(kTransparencyId, image.width, image.height, image.depth, is_psb));
        for (std::size_t i = 0; i < color_count; ++i)
            channels.push_back(encode(static_cast<std::int16_t>(i), i));
        return channels;
    }

    Rect BoundsAt(const ImageView& image, std::int32_t top, std::int32_t left)
    {
        if (image.IsEmpty())
            return {top, left, top, left};
        return CheckedRect(top, left, std::int64_t{top} + image.height, std::int64_t{left} + image.width);
    }

    ImageInfo LayerPixelsInfo(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth)
    {
        return FindLayerPlanes(record, color_mode, depth).info;
    }

    Image DecodeLayerPixels(const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb)
    {
        Image image(LayerPixelsInfo(record, color_mode, depth));
        DecodeLayerPixels(record, color_mode, depth, is_psb, image.bytes.data(), image.bytes.size());
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

    void ReplaceLayerPixels(LayerRecord& record, const ImageView& image, ColorMode color_mode, bool is_background, bool is_psb)
    {
        std::vector<ChannelImageData> channels = EncodeLayerPixels(image, color_mode, is_background, is_psb);
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

} // namespace ffpsd::detail
