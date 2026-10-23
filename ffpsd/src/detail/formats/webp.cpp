#include "detail/formats/webp.hpp"

#include "detail/color.hpp"
#include "detail/formats/picture.hpp"
#include "detail/formats/planes.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/bytes.hpp>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <webp/decode.h>
#include <webp/encode.h>

namespace ffpsd::detail
{
    namespace
    {
        constexpr const char* kVersionMismatch = "ffpsd: WebP: the libwebp headers do not match the library";

        // The output buffer is ours, so freeing it only drops what libwebp keeps beside it.
        struct Decoder
        {
            WebPDecoderConfig config = {};

            Decoder()
            {
                if (WebPInitDecoderConfig(&config) == 0)
                    throw std::runtime_error(kVersionMismatch);
            }
            Decoder(const Decoder& other) = delete;
            Decoder& operator=(const Decoder& other) = delete;
            ~Decoder()
            {
                WebPFreeDecBuffer(&config.output);
            }
        };

        struct Encoder
        {
            WebPConfig config = {};
            WebPPicture picture = {};
            WebPMemoryWriter writer = {};

            Encoder()
            {
                if (WebPConfigInit(&config) == 0 || WebPPictureInit(&picture) == 0)
                    throw std::runtime_error(kVersionMismatch);
                WebPMemoryWriterInit(&writer);
            }
            Encoder(const Encoder& other) = delete;
            Encoder& operator=(const Encoder& other) = delete;
            ~Encoder()
            {
                WebPPictureFree(&picture);
                WebPMemoryWriterClear(&writer);
            }
        };

        void CheckStatus(VP8StatusCode status)
        {
            switch (status)
            {
            case VP8_STATUS_OK:
                return;
            case VP8_STATUS_OUT_OF_MEMORY:
                throw std::bad_alloc();
            case VP8_STATUS_NOT_ENOUGH_DATA:
                throw std::runtime_error("ffpsd: WebP: unexpected end of data");
            case VP8_STATUS_UNSUPPORTED_FEATURE:
                throw std::runtime_error("ffpsd: WebP: a feature libwebp does not decode");
            default:
                throw std::runtime_error("ffpsd: WebP: broken data, status " + std::to_string(static_cast<int>(status)));
            }
        }

        [[noreturn]] void ThrowEncodingError(WebPEncodingError error)
        {
            switch (error)
            {
            case VP8_ENC_ERROR_OUT_OF_MEMORY:
            case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
                throw std::bad_alloc();
            case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
                throw std::runtime_error("ffpsd: WebP: the picture's header data is over 512 KB, lower the quality");
            case VP8_ENC_ERROR_FILE_TOO_BIG:
                throw std::runtime_error("ffpsd: WebP: the file would be over 4 GB");
            default:
                throw std::runtime_error("ffpsd: WebP: encoding failed, error " + std::to_string(static_cast<int>(error)));
            }
        }

        // One interleaved 8 bit row into libwebp's 0xAARRGGBB pixels: gray goes to all three colors, no alpha is opaque.
        void
        PackRow(const std::uint8_t* samples, std::uint32_t width, std::uint16_t channels, std::uint16_t colors, std::uint32_t* out) noexcept
        {
            const std::size_t green = colors == 3 ? 1 : 0;
            const std::size_t blue = colors == 3 ? 2 : 0;
            for (std::uint32_t x = 0; x < width; ++x, samples += channels)
            {
                const std::uint32_t alpha = channels > colors ? samples[colors] : 0xFFu;
                out[x] = alpha << 24 | std::uint32_t{samples[0]} << 16 | std::uint32_t{samples[green]} << 8 | samples[blue];
            }
        }
    } // namespace

    Image DecodeWebp(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth)
    {
        CheckPictureMode(color_mode, depth, "WebP");
        if (FindFormat(data, size) != Format::kWebp)
            throw std::runtime_error("ffpsd: not a WebP");

        Decoder decoder;
        WebPDecoderConfig& config = decoder.config;
        CheckStatus(WebPGetFeatures(data, size, &config.input));
        if (config.input.has_animation != 0)
            throw std::runtime_error("ffpsd: an animated WebP is not supported");

        // libwebp writes every byte straight into ours.
        const auto width = static_cast<std::uint32_t>(config.input.width);
        const auto height = static_cast<std::uint32_t>(config.input.height);
        const std::uint16_t channels = config.input.has_alpha != 0 ? 4 : 3;
        Bytes pixels(std::size_t{width} * height * channels);
        config.output.colorspace = channels == 4 ? MODE_RGBA : MODE_RGB;
        config.output.is_external_memory = 1;
        config.output.u.RGBA.rgba = pixels.data();
        config.output.u.RGBA.stride = static_cast<int>(width * channels);
        config.output.u.RGBA.size = pixels.size();
        CheckStatus(WebPDecode(data, size, &config));

        Image image = ConvertColorMode(Deinterleave(pixels.data(), width, height, channels, 8), color_mode);
        if (depth == 16)
            return WidenTo16(image);
        return image;
    }

    std::vector<std::uint8_t> EncodeWebp(const ImageView& image, int quality, bool lossless)
    {
        if (quality < 0 || quality > 100)
            throw std::invalid_argument("ffpsd: WebP quality is 0 to 100, not " + std::to_string(quality));
        if (image.width > WEBP_MAX_DIMENSION || image.height > WEBP_MAX_DIMENSION)
            throw std::invalid_argument("ffpsd: a WebP side is at most " + std::to_string(WEBP_MAX_DIMENSION) + " pixels");
        CheckPicture(image, "WebP");

        // exact keeps the colors under transparent pixels, which lossless would otherwise change for size.
        Encoder encoder;
        encoder.config.quality = static_cast<float>(quality);
        encoder.config.lossless = lossless ? 1 : 0;
        encoder.config.exact = lossless ? 1 : 0;
        encoder.picture.use_argb = 1;
        encoder.picture.width = static_cast<int>(image.width);
        encoder.picture.height = static_cast<int>(image.height);
        encoder.picture.writer = WebPMemoryWrite;
        encoder.picture.custom_ptr = &encoder.writer;
        if (WebPPictureAlloc(&encoder.picture) == 0)
            throw std::bad_alloc();

        // A row at a time, so the interleaved picture is never held whole.
        const std::uint16_t colors = ColorChannelCount(image.color_mode);
        std::vector<std::uint8_t> row(std::size_t{image.width} * image.channel_count * image.GetBytesPerSample());
        std::vector<std::uint8_t> narrow(image.depth == 16 ? std::size_t{image.width} * image.channel_count : 0);
        for (std::uint32_t y = 0; y < image.height; ++y)
        {
            InterleaveRow(image, y, row.data());
            const std::uint8_t* samples = row.data();
            if (image.depth == 16)
            {
                NarrowRow(row.data(), narrow.size(), narrow.data());
                samples = narrow.data();
            }
            std::uint32_t* argb_row = encoder.picture.argb + std::size_t{y} * static_cast<std::size_t>(encoder.picture.argb_stride);
            PackRow(samples, image.width, image.channel_count, colors, argb_row);
        }

        if (WebPEncode(&encoder.config, &encoder.picture) == 0)
            ThrowEncodingError(encoder.picture.error_code);
        return std::vector<std::uint8_t>(encoder.writer.mem, encoder.writer.mem + encoder.writer.size);
    }
} // namespace ffpsd::detail
