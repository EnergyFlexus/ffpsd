#include "detail/formats/jpeg.hpp"

#include "detail/color.hpp"
#include "detail/formats/exif.hpp"
#include "detail/formats/picture.hpp"
#include "detail/formats/planes.hpp"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstdio> // jpeglib.h uses FILE and size_t without including them
#include <cstdlib>
#include <cstring>
#include <jpeglib.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    namespace
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324) // jmp_buf is 16 byte aligned on x64, so the struct is padded
#endif
        // libjpeg reports through error_exit, which must not return, so its message waits here until setjmp returns.
        struct Error
        {
            jpeg_error_mgr manager; // first, so libjpeg's pointer to it is a pointer to this
            std::jmp_buf jump;
            char message[JMSG_LENGTH_MAX];
        };
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        [[noreturn]] void OnError(j_common_ptr info)
        {
            Error* error = reinterpret_cast<Error*>(info->err);
            (*info->err->format_message)(info, error->message);
            std::longjmp(error->jump, 1);
        }

        // Warnings would go to stderr.
        void OnOutput(j_common_ptr)
        {
        }

        jpeg_error_mgr* Attach(Error& error)
        {
            jpeg_error_mgr* manager = jpeg_std_error(&error.manager);
            manager->error_exit = OnError;
            manager->output_message = OnOutput;
            return manager;
        }

        struct Decompressor
        {
            jpeg_decompress_struct info = {};
            Error error = {};

            Decompressor()
            {
                info.err = Attach(error);
            }
            Decompressor(const Decompressor& other) = delete;
            Decompressor& operator=(const Decompressor& other) = delete;
            ~Decompressor()
            {
                jpeg_destroy_decompress(&info);
            }
        };

        struct Decoded
        {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint16_t channels = 0;
            bool adobe = false; // Adobe's CMYK is stored inverted
            Orientation orientation = Orientation::kNormal;
            std::unique_ptr<std::uint8_t[]> bytes; // not zeroed first: libjpeg writes every byte
        };

        struct Compressor
        {
            jpeg_compress_struct info = {};
            Error error = {};
            unsigned char* buffer = nullptr; // jpeg_mem_dest's, from malloc
            unsigned long size = 0;

            Compressor()
            {
                info.err = Attach(error);
            }
            Compressor(const Compressor& other) = delete;
            Compressor& operator=(const Compressor& other) = delete;
            ~Compressor()
            {
                jpeg_destroy_compress(&info);
                std::free(buffer);
            }
        };

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4611) // setjmp next to C++ objects: none of them lives in this frame
#endif
        // Everything that outlives a longjmp belongs to the caller, so the jump skips no destructor.
        bool Decode(Decompressor& decompressor, const std::uint8_t* data, std::size_t size, Decoded& out)
        {
            j_decompress_ptr info = &decompressor.info;
            if (setjmp(decompressor.error.jump))
                return false;

            jpeg_CreateDecompress(info, JPEG_LIB_VERSION, sizeof(jpeg_decompress_struct));
            jpeg_mem_src(info, data, static_cast<unsigned long>(size));
            jpeg_save_markers(info, JPEG_APP0 + 1, 0xFFFF);
            jpeg_read_header(info, TRUE);

            // XMP shares APP1 and decodes as normal, so the first turned one wins.
            for (jpeg_saved_marker_ptr marker = info->marker_list; marker != nullptr; marker = marker->next)
            {
                if (out.orientation == Orientation::kNormal)
                    out.orientation = DecodeExifOrientation(marker->data, marker->data_length);
            }

            switch (info->jpeg_color_space)
            {
            case JCS_GRAYSCALE:
                info->out_color_space = JCS_GRAYSCALE;
                break;
            case JCS_CMYK:
            case JCS_YCCK:
                info->out_color_space = JCS_CMYK;
                break;
            default:
                info->out_color_space = JCS_RGB;
                break;
            }
            jpeg_start_decompress(info);

            out.width = info->output_width;
            out.height = info->output_height;
            out.channels = static_cast<std::uint16_t>(info->output_components);
            out.adobe = info->saw_Adobe_marker != 0;

            const std::size_t row_bytes = std::size_t{out.width} * out.channels;
            out.bytes.reset(new std::uint8_t[row_bytes * out.height]);
            while (info->output_scanline < info->output_height)
            {
                JSAMPROW row = out.bytes.get() + std::size_t{info->output_scanline} * row_bytes;
                jpeg_read_scanlines(info, &row, 1);
            }
            jpeg_finish_decompress(info);
            return true;
        }

        bool Encode(
            Compressor& compressor, const std::vector<std::uint8_t>& pixels, std::uint32_t width, std::uint32_t height,
            std::uint16_t channels, int quality)
        {
            j_compress_ptr info = &compressor.info;
            if (setjmp(compressor.error.jump))
                return false;

            jpeg_CreateCompress(info, JPEG_LIB_VERSION, sizeof(jpeg_compress_struct));
            jpeg_mem_dest(info, &compressor.buffer, &compressor.size);
            info->image_width = width;
            info->image_height = height;
            info->input_components = channels;
            info->in_color_space = channels == 1 ? JCS_GRAYSCALE : JCS_RGB;
            jpeg_set_defaults(info);
            jpeg_set_quality(info, quality, TRUE);
            info->optimize_coding = TRUE;
            jpeg_start_compress(info, TRUE);

            const std::size_t row_bytes = std::size_t{width} * channels;
            while (info->next_scanline < info->image_height)
            {
                JSAMPROW row = const_cast<JSAMPROW>(pixels.data() + std::size_t{info->next_scanline} * row_bytes);
                jpeg_write_scanlines(info, &row, 1);
            }
            jpeg_finish_compress(info);
            return true;
        }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        // Each ink times black, as light left over; no color profiles.
        void CmykToRgb(Decoded& decoded)
        {
            const std::size_t pixels = std::size_t{decoded.width} * decoded.height;
            std::uint8_t* bytes = decoded.bytes.get();
            for (std::size_t i = 0; i < pixels; ++i)
            {
                unsigned light[4];
                for (std::size_t ink = 0; ink < 4; ++ink)
                    light[ink] = decoded.adobe ? bytes[i * 4 + ink] : 255u - bytes[i * 4 + ink];
                for (std::size_t color = 0; color < 3; ++color)
                    bytes[i * 3 + color] = static_cast<std::uint8_t>((light[color] * light[3] + 127) / 255);
            }
            decoded.channels = 3; // the bytes past three a pixel stay unused
        }

        Image WidenTo16(const Image& image)
        {
            Image wide = image;
            wide.depth = 16;
            wide.bytes.resize(image.bytes.size() * 2);
            for (std::size_t i = 0; i < image.bytes.size(); ++i)
            {
                const auto sample = static_cast<std::uint16_t>(image.bytes[i] * 257);
                std::memcpy(wide.bytes.data() + i * 2, &sample, 2);
            }
            return wide;
        }

        // Rounded, as libpng narrows 16 bit.
        std::vector<std::uint8_t> NarrowTo8(const std::vector<std::uint8_t>& samples)
        {
            std::vector<std::uint8_t> narrow(samples.size() / 2);
            for (std::size_t i = 0; i < narrow.size(); ++i)
            {
                std::uint16_t sample = 0;
                std::memcpy(&sample, samples.data() + i * 2, 2);
                narrow[i] = static_cast<std::uint8_t>((sample * 255u + 32767) / 65535);
            }
            return narrow;
        }
    } // namespace

    Image DecodeJpeg(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth, bool apply_orientation)
    {
        CheckPictureMode(color_mode, depth, "JPEG");
        if (data == nullptr || size < 3 || data[0] != 0xFF || data[1] != 0xD8 || data[2] != 0xFF)
            throw std::runtime_error("ffpsd: not a JPEG");
        if (size > std::numeric_limits<unsigned long>::max())
            throw std::runtime_error("ffpsd: JPEG is larger than libjpeg reads");

        Decompressor decompressor;
        Decoded decoded;
        if (!Decode(decompressor, data, size, decoded))
            throw std::runtime_error(std::string("ffpsd: JPEG: ") + decompressor.error.message);
        if (decoded.channels == 4)
            CmykToRgb(decoded);

        const Orientation orientation = apply_orientation ? decoded.orientation : Orientation::kNormal;
        Image image = ConvertColorMode(
            Deinterleave(decoded.bytes.get(), decoded.width, decoded.height, decoded.channels, 8, orientation), color_mode);
        if (depth == 16)
            return WidenTo16(image);
        return image;
    }

    std::vector<std::uint8_t> EncodeJpeg(const ImageView& image, int quality)
    {
        if (quality < 1 || quality > 100)
            throw std::invalid_argument("ffpsd: JPEG quality is 1 to 100, not " + std::to_string(quality));
        if (image.width > JPEG_MAX_DIMENSION || image.height > JPEG_MAX_DIMENSION)
            throw std::invalid_argument("ffpsd: a JPEG side is at most " + std::to_string(JPEG_MAX_DIMENSION) + " pixels");
        CheckPicture(image, "JPEG");

        const std::uint16_t colors = ColorChannelCount(image.color_mode);
        std::vector<std::uint8_t> pixels = Interleave(image, colors);
        if (image.depth == 16)
            pixels = NarrowTo8(pixels);

        Compressor compressor;
        if (!Encode(compressor, pixels, image.width, image.height, colors, quality))
            throw std::runtime_error(std::string("ffpsd: JPEG: ") + compressor.error.message);
        return std::vector<std::uint8_t>(compressor.buffer, compressor.buffer + compressor.size);
    }
} // namespace ffpsd::detail
