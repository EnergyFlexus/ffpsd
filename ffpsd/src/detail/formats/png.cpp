#include "detail/formats/png.hpp"

#include "detail/color.hpp"
#include "detail/formats/picture.hpp"
#include "detail/formats/planes.hpp"
#include "detail/image.hpp"
#include "detail/io/byte_order.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <png.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kSignatureSize = 8;
        constexpr std::uint32_t kMaxSide = 300000;
        constexpr std::size_t kErrorSize = 256;

        struct Source
        {
            const std::uint8_t* data = nullptr;
            std::size_t size = 0;
            std::size_t offset = 0;
        };

        // libpng reports through longjmp, so its message waits here until setjmp returns.
        struct Reader
        {
            png_structp png = nullptr;
            png_infop info = nullptr;
            Source source;
            char error[kErrorSize] = {};

            Reader() = default;
            Reader(const Reader& other) = delete;
            Reader& operator=(const Reader& other) = delete;
            ~Reader()
            {
                png_destroy_read_struct(&png, &info, nullptr);
            }
        };

        struct Decoded
        {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint8_t channels = 0;

            // Rows of a progressive picture come in passes, so it is read whole, without zeros first, then split.
            std::unique_ptr<std::uint8_t[]> bytes;
            std::vector<png_bytep> rows;

            // Any other goes row by row straight into its planes.
            std::vector<std::uint8_t> row;
            Image planes;
        };

        void ReadData(png_structp png, png_bytep out, png_size_t count)
        {
            auto* source = static_cast<Source*>(png_get_io_ptr(png));
            if (count > source->size - source->offset)
                png_error(png, "unexpected end of data");

            std::memcpy(out, source->data + source->offset, count);
            source->offset += count;
        }

        // The error pointer is the char[kErrorSize] of a Reader or a Writer.
        [[noreturn]] void OnError(png_structp png, png_const_charp message)
        {
            std::snprintf(static_cast<char*>(png_get_error_ptr(png)), kErrorSize, "%s", message);
            png_longjmp(png, 1);
        }

        void OnWarning(png_structp, png_const_charp)
        {
        }

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4611) // setjmp next to C++ objects: none of them lives in this frame
#endif
        // Everything that outlives a longjmp belongs to the caller, so the jump skips no destructor.
        bool Decode(Reader& reader, std::uint16_t depth, Decoded& out)
        {
            png_structp png = reader.png;
            png_infop info = reader.info;
            if (setjmp(png_jmpbuf(png)))
                return false;

            png_set_read_fn(png, &reader.source, ReadData);
            png_read_info(png, info);

            const png_byte color_type = png_get_color_type(png, info);
            if (color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_palette_to_rgb(png);
            if (color_type == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png, info) < 8)
                png_set_expand_gray_1_2_4_to_8(png);
            if (png_get_valid(png, info, PNG_INFO_tRNS))
                png_set_tRNS_to_alpha(png);

            if (depth == 8)
                png_set_scale_16(png);
            else
                png_set_expand_16(png);

            // PNG stores 16 bit samples big endian, Image keeps native order.
            if (depth == 16 && kNativeLittle)
                png_set_swap(png);

            png_set_interlace_handling(png);
            png_read_update_info(png, info);

            out.width = png_get_image_width(png, info);
            out.height = png_get_image_height(png, info);
            out.channels = png_get_channels(png, info);
            if (out.width > kMaxSide || out.height > kMaxSide)
                png_error(png, "image is larger than a PSB allows");

            const std::size_t row_bytes = png_get_rowbytes(png, info);
            if (png_get_interlace_type(png, info) == PNG_INTERLACE_NONE)
            {
                out.row.resize(row_bytes);
                out.planes = MakePlanes(out.width, out.height, out.channels, depth);
                for (std::uint32_t y = 0; y < out.height; ++y)
                {
                    png_read_row(png, out.row.data(), nullptr);
                    DeinterleaveRow(out.row.data(), out.planes, y);
                }
            }
            else
            {
                out.bytes.reset(new std::uint8_t[row_bytes * out.height]);
                out.rows.resize(out.height);
                for (std::size_t y = 0; y < out.height; ++y)
                    out.rows[y] = out.bytes.get() + y * row_bytes;
                png_read_image(png, out.rows.data());
            }
            png_read_end(png, nullptr);
            return true;
        }

        struct Writer
        {
            png_structp png = nullptr;
            png_infop info = nullptr;
            std::vector<std::uint8_t>* out = nullptr;
            char error[kErrorSize] = {};

            Writer() = default;
            Writer(const Writer& other) = delete;
            Writer& operator=(const Writer& other) = delete;
            ~Writer()
            {
                png_destroy_write_struct(&png, &info);
            }
        };

        // Interleaved rows ready for libpng, built before the setjmp.
        struct Interleaved
        {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            int bit_depth = 8;
            int color_type = PNG_COLOR_TYPE_GRAY;
            std::vector<std::uint8_t> bytes;
            std::vector<png_bytep> rows;
        };

        void WriteData(png_structp png, png_bytep data, png_size_t count)
        {
            auto* writer = static_cast<Writer*>(png_get_io_ptr(png));
            try
            {
                writer->out->insert(writer->out->end(), data, data + count);
            }
            catch (...)
            {
                // An exception must not unwind through libpng's C frames.
                png_error(png, "out of memory");
            }
        }

        void FlushData(png_structp)
        {
        }

        bool Encode(Writer& writer, Interleaved& image)
        {
            png_structp png = writer.png;
            png_infop info = writer.info;
            if (setjmp(png_jmpbuf(png)))
                return false;

            png_set_write_fn(png, &writer, WriteData, FlushData);
            png_set_IHDR(
                png, info, image.width, image.height, image.bit_depth, image.color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
            png_write_info(png, info);

            if (image.bit_depth == 16 && kNativeLittle)
                png_set_swap(png);

            png_write_image(png, image.rows.data());
            png_write_end(png, nullptr);
            return true;
        }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        int ColorType(const ImageView& image)
        {
            const bool has_alpha = HasTransparency(image);
            if (image.color_mode == ColorMode::kGrayscale)
                return has_alpha ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY;
            return has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
        }
    } // namespace

    Image DecodePng(const std::uint8_t* data, std::size_t size, ColorMode color_mode, std::uint16_t depth)
    {
        CheckPictureMode(color_mode, depth, "PNG");
        if (data == nullptr || size < kSignatureSize || png_sig_cmp(data, 0, kSignatureSize) != 0)
            throw std::runtime_error("ffpsd: not a PNG");

        Reader reader;
        reader.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, reader.error, OnError, OnWarning);
        if (reader.png == nullptr)
            throw std::bad_alloc();
        reader.info = png_create_info_struct(reader.png);
        if (reader.info == nullptr)
            throw std::bad_alloc();
        reader.source.data = data;
        reader.source.size = size;

        Decoded decoded;
        if (!Decode(reader, depth, decoded))
            throw std::runtime_error(std::string("ffpsd: PNG: ") + reader.error);

        if (decoded.bytes)
            decoded.planes = Deinterleave(decoded.bytes.get(), decoded.width, decoded.height, decoded.channels, depth);
        return ConvertColorMode(std::move(decoded.planes), color_mode);
    }

    std::vector<std::uint8_t> EncodePng(const ImageView& image)
    {
        if (image.width > kMaxSide || image.height > kMaxSide)
            throw std::invalid_argument("ffpsd: image is larger than a PSB allows");
        CheckPicture(image, "PNG");

        Interleaved interleaved;
        interleaved.width = image.width;
        interleaved.height = image.height;
        interleaved.bit_depth = image.depth;
        interleaved.color_type = ColorType(image);
        interleaved.bytes = Interleave(image, image.channel_count);

        const std::size_t row_bytes = std::size_t{image.width} * image.channel_count * image.GetBytesPerSample();
        interleaved.rows.resize(image.height);
        for (std::size_t y = 0; y < image.height; ++y)
            interleaved.rows[y] = interleaved.bytes.data() + y * row_bytes;

        std::vector<std::uint8_t> out;
        Writer writer;
        writer.out = &out;
        writer.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, writer.error, OnError, OnWarning);
        if (writer.png == nullptr)
            throw std::bad_alloc();
        writer.info = png_create_info_struct(writer.png);
        if (writer.info == nullptr)
            throw std::bad_alloc();

        if (!Encode(writer, interleaved))
            throw std::runtime_error(std::string("ffpsd: PNG: ") + writer.error);
        return out;
    }
} // namespace ffpsd::detail
