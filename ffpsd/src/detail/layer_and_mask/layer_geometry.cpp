#include "detail/layer_and_mask/layer_geometry.hpp"

#include "detail/composite.hpp"
#include "detail/image.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/layer_pixels.hpp"
#include "detail/resample.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace ffpsd::detail
{
    namespace
    {
        struct CanvasBlock
        {
            std::uint32_t key;
            const char* what;
            bool fractions; // its coordinates are fractions of the canvas size
        };

        constexpr CanvasBlock kCanvasBlocks[] = {
            {Fourcc('T', 'y', 'S', 'h'), "text", false},          {Fourcc('t', 'y', 'S', 'h'), "text", false},
            {Fourcc('S', 'o', 'L', 'd'), "smart objects", false}, {Fourcc('S', 'o', 'L', 'E'), "smart objects", false},
            {Fourcc('P', 'l', 'L', 'd'), "smart objects", false}, {Fourcc('p', 'l', 'L', 'd'), "smart objects", false},
            {Fourcc('v', 'o', 'g', 'k'), "shapes", false},        {Fourcc('v', 'm', 's', 'k'), "vector masks", true},
            {Fourcc('v', 's', 'm', 's'), "vector masks", true},
        };

        constexpr std::int16_t kMaskIds[] = {kLayerMaskId, kRealMaskId};

        Image TurnImage(const Image& image, bool transpose, bool flip_x, bool flip_y)
        {
            Image result = MakeImage(
                transpose ? image.height : image.width, transpose ? image.width : image.height, image.channel_count, image.depth,
                image.color_mode);

            const std::size_t sample = image.GetBytesPerSample();
            const std::size_t plane = std::size_t{image.width} * image.height * sample;
            for (std::size_t channel = 0; channel < image.channel_count; ++channel)
            {
                const std::uint8_t* in = image.bytes.data() + channel * plane;
                std::uint8_t* out = result.bytes.data() + channel * plane;
                for (std::size_t y = 0; y < result.height; ++y)
                {
                    for (std::size_t x = 0; x < result.width; ++x)
                    {
                        const std::size_t across = flip_x ? result.width - 1 - x : x;
                        const std::size_t down = flip_y ? result.height - 1 - y : y;
                        const std::size_t from = transpose ? across * image.width + down : down * image.width + across;
                        std::memcpy(out + (y * result.width + x) * sample, in + from * sample, sample);
                    }
                }
            }
            return result;
        }
    } // namespace

    Transform Transform::Shift(double dx, double dy) noexcept
    {
        return {1, 0, 0, 1, dx, dy};
    }

    Transform Transform::Scale(double sx, double sy, double origin_x, double origin_y) noexcept
    {
        return {sx, 0, 0, sy, origin_x * (1 - sx), origin_y * (1 - sy)};
    }

    Transform Transform::Rotate(Rotation rotation, const Rect& frame) noexcept
    {
        const auto width = static_cast<double>(frame.GetWidth());
        const auto height = static_cast<double>(frame.GetHeight());
        Transform turn;
        switch (rotation)
        {
        case Rotation::k90:
            turn = {0, 1, -1, 0, height, 0};
            break;
        case Rotation::k180:
            turn = {-1, 0, 0, -1, width, height};
            break;
        default:
            turn = {0, -1, 1, 0, 0, width};
            break;
        }
        return Shift(-frame.left, -frame.top).Then(turn).Then(Shift(frame.left, frame.top));
    }

    Transform Transform::Flip(FlipDirection direction, const Rect& frame) noexcept
    {
        const auto width = static_cast<double>(frame.GetWidth());
        const auto height = static_cast<double>(frame.GetHeight());
        const Transform mirror =
            direction == FlipDirection::kHorizontal ? Transform{-1, 0, 0, 1, width, 0} : Transform{1, 0, 0, -1, 0, height};
        return Shift(-frame.left, -frame.top).Then(mirror).Then(Shift(frame.left, frame.top));
    }

    Transform Transform::Then(const Transform& next) const noexcept
    {
        return {
            next.a * a + next.c * b,
            next.b * a + next.d * b,
            next.a * c + next.c * d,
            next.b * c + next.d * d,
            next.a * tx + next.c * ty + next.tx,
            next.b * tx + next.d * ty + next.ty};
    }

    Rect TransformRect(const Rect& rect, const Transform& transform)
    {
        const auto x = [&](double px, double py) { return transform.a * px + transform.c * py + transform.tx; };
        const auto y = [&](double px, double py) { return transform.b * px + transform.d * py + transform.ty; };
        const double x1 = x(rect.left, rect.top);
        const double x2 = x(rect.right, rect.bottom);
        const double y1 = y(rect.left, rect.top);
        const double y2 = y(rect.right, rect.bottom);

        const std::int64_t left = std::llround(std::min(x1, x2));
        const std::int64_t top = std::llround(std::min(y1, y2));
        std::int64_t right = std::llround(std::max(x1, x2));
        std::int64_t bottom = std::llround(std::max(y1, y2));
        if (rect.GetWidth() > 0 && rect.GetHeight() > 0)
        {
            right = std::max(right, left + 1);
            bottom = std::max(bottom, top + 1);
        }
        return MakeRect(top, left, bottom, right);
    }

    Image TransformPixels(
        Image image, const Transform& transform, std::uint32_t width, std::uint32_t height, ResampleFilter filter, bool planes_alone)
    {
        const bool transpose = transform.SwapsAxes();
        const bool flip_x = (transpose ? transform.c : transform.a) < 0;
        const bool flip_y = (transpose ? transform.b : transform.d) < 0;
        if (transpose || flip_x || flip_y)
            image = TurnImage(image, transpose, flip_x, flip_y);
        if (image.width == width && image.height == height)
            return image;
        return Resample(image, width, height, filter, planes_alone);
    }

    void CheckCanvasBlocks(const LayerRecord& record, bool keeps_fractions)
    {
        for (const CanvasBlock& block : kCanvasBlocks)
        {
            if (!(keeps_fractions && block.fractions) && FindTaggedBlock(record.blocks, block.key) != nullptr)
                throw std::logic_error(
                    std::string("ffpsd: layer '") + record.name + "' has " + block.what + ", whose placement ffpsd does not update yet");
        }
    }

    void ApplyTransformed(LayerRecord& record, TransformedLayer&& transformed)
    {
        record.bounds = transformed.bounds;
        if (transformed.channels.has_value())
            record.channels = std::move(*transformed.channels);
        record.mask_data = std::move(transformed.mask_data);
    }

    TransformedLayer TransformLayer(
        const LayerRecord& record, ColorMode color_mode, std::uint16_t depth, bool is_psb, const Transform& transform,
        ResampleFilter filter, const Rect* canvas)
    {
        TransformedLayer result;
        result.bounds = TransformRect(record.bounds, transform);
        result.mask_data = record.mask_data;
        for (const std::int16_t id : kMaskIds)
        {
            if (const std::optional<Rect> bounds = FindMaskBounds(record.mask_data, id))
                SetMaskBounds(result.mask_data, id, TransformRect(*bounds, transform));
        }
        if (transform.IsShift() && canvas == nullptr)
            return result;

        const auto width = static_cast<std::uint32_t>(result.bounds.GetWidth());
        const auto height = static_cast<std::uint32_t>(result.bounds.GetHeight());
        CheckLayerSides(width, height, is_psb);

        std::vector<ChannelImageData> channels;
        Image pixels = DecodeLayerPixels(record, color_mode, depth, is_psb);
        if (!pixels.IsEmpty())
        {
            const Image moved = TransformPixels(std::move(pixels), transform, width, height, filter, false);
            if (canvas != nullptr)
            {
                Image white = MakeWhiteImage(
                    static_cast<std::uint32_t>(canvas->GetWidth()), static_cast<std::uint32_t>(canvas->GetHeight()), color_mode, depth);
                PlaceImage(moved, white, std::int64_t{result.bounds.top} - canvas->top, std::int64_t{result.bounds.left} - canvas->left);
                result.bounds = *canvas;
                channels = EncodeLayerPixels(white, true, is_psb);
            }
            else
            {
                channels = EncodeLayerPixels(moved, false, is_psb);
            }
        }
        else
        {
            // Groups, adjustments and empty layers: their channels are compression fields only.
            for (const ChannelImageData& channel : record.channels)
            {
                if (channel.id >= kTransparencyId)
                    channels.push_back(channel);
            }
        }

        for (const ChannelImageData& channel : record.channels)
        {
            if (channel.id >= kTransparencyId)
                continue;

            const Rect from = RequireMaskBounds(record.mask_data, channel.id);
            const Rect to = RequireMaskBounds(result.mask_data, channel.id);
            Image mask = DecodeMask(channel, from, depth, is_psb);
            if (mask.IsEmpty())
            {
                channels.push_back(channel);
                continue;
            }
            const auto mask_width = static_cast<std::uint32_t>(to.GetWidth());
            const auto mask_height = static_cast<std::uint32_t>(to.GetHeight());
            CheckLayerSides(mask_width, mask_height, is_psb);
            const Image moved = TransformPixels(std::move(mask), transform, mask_width, mask_height, filter, true);
            channels.push_back(EncodeChannelImageData(channel.id, moved.bytes.data(), mask_width, mask_height, depth, is_psb));
        }
        result.channels = std::move(channels);
        return result;
    }
} // namespace ffpsd::detail
