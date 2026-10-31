#include "common.hpp"

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <vector>

namespace ffpsd_python
{
    using namespace nb::literals;

    namespace
    {
        // Decodes with the GIL released; the result becomes an array once it is back.
        template <class Decode> NumpyArray Decoded(Decode&& decode)
        {
            ffpsd::Image image;
            {
                nb::gil_scoped_release release;
                image = decode();
            }
            return ToArray(std::move(image));
        }

        template <class Encode> nb::bytes Encoded(Encode&& encode)
        {
            std::vector<std::uint8_t> data;
            {
                nb::gil_scoped_release release;
                data = encode();
            }
            return ToBytes(data.data(), data.size());
        }

        template <class Save> void Saved(Save&& save)
        {
            nb::gil_scoped_release release;
            save();
        }

        [[noreturn]] void Lacks(const char* format)
        {
            throw Unsupported(std::string("ffpsd: built without ") + format + " support");
        }
    } // namespace

    void BindFormats(nb::module_& m)
    {
        m.def("is_format_supported", &ffpsd::IsFormatSupported, "format"_a);

        m.def(
            "load_picture",
            [](const std::filesystem::path& path, ffpsd::ColorMode color_mode, std::uint16_t depth) {
                const std::string utf8 = ToUtf8(path);
                return Decoded([&] { return ffpsd::LoadPicture(utf8, color_mode, depth); });
            },
            "path"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);
        m.def(
            "decode_picture",
            [](const BytesLike& data, ffpsd::ColorMode color_mode, std::uint16_t depth) {
                return Decoded([&] { return ffpsd::LoadPicture(data.data(), data.size(), color_mode, depth); });
            },
            "data"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);

        m.def(
            "load_png",
            [](const std::filesystem::path& path, ffpsd::ColorMode color_mode, std::uint16_t depth) -> NumpyArray {
#if defined(FFPSD_HAS_PNG)
                const std::string utf8 = ToUtf8(path);
                return Decoded([&] { return ffpsd::LoadPng(utf8, color_mode, depth); });
#else
                Lacks("PNG");
#endif
            },
            "path"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);
        m.def(
            "decode_png",
            [](const BytesLike& data, ffpsd::ColorMode color_mode, std::uint16_t depth) -> NumpyArray {
#if defined(FFPSD_HAS_PNG)
                return Decoded([&] { return ffpsd::LoadPng(data.data(), data.size(), color_mode, depth); });
#else
                Lacks("PNG");
#endif
            },
            "data"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);
        m.def(
            "encode_png",
            [](const PixelArray& pixels, ffpsd::PngCompression compression, std::optional<ffpsd::ColorMode> color_mode) -> nb::bytes {
#if defined(FFPSD_HAS_PNG)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                return Encoded([&] { return ffpsd::EncodePng(view, compression); });
#else
                Lacks("PNG");
#endif
            },
            "pixels"_a, "compression"_a = ffpsd::PngCompression::kBalanced, "color_mode"_a = nb::none());
        m.def(
            "save_png",
            [](const PixelArray& pixels, const std::filesystem::path& path, ffpsd::PngCompression compression,
               std::optional<ffpsd::ColorMode> color_mode) {
#if defined(FFPSD_HAS_PNG)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                const std::string utf8 = ToUtf8(path);
                Saved([&] { ffpsd::SavePng(view, utf8, compression); });
#else
                Lacks("PNG");
#endif
            },
            "pixels"_a, "path"_a, "compression"_a = ffpsd::PngCompression::kBalanced, "color_mode"_a = nb::none());

        m.def(
            "load_jpeg",
            [](const std::filesystem::path& path, ffpsd::ColorMode color_mode, std::uint16_t depth, bool apply_orientation) -> NumpyArray {
#if defined(FFPSD_HAS_JPEG)
                const std::string utf8 = ToUtf8(path);
                return Decoded([&] { return ffpsd::LoadJpeg(utf8, color_mode, depth, apply_orientation); });
#else
                Lacks("JPEG");
#endif
            },
            "path"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8, "apply_orientation"_a = true);
        m.def(
            "decode_jpeg",
            [](const BytesLike& data, ffpsd::ColorMode color_mode, std::uint16_t depth, bool apply_orientation) -> NumpyArray {
#if defined(FFPSD_HAS_JPEG)
                return Decoded([&] { return ffpsd::LoadJpeg(data.data(), data.size(), color_mode, depth, apply_orientation); });
#else
                Lacks("JPEG");
#endif
            },
            "data"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8, "apply_orientation"_a = true);
        m.def(
            "encode_jpeg",
            [](const PixelArray& pixels, int quality, std::optional<ffpsd::ColorMode> color_mode) -> nb::bytes {
#if defined(FFPSD_HAS_JPEG)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                return Encoded([&] { return ffpsd::EncodeJpeg(view, quality); });
#else
                Lacks("JPEG");
#endif
            },
            "pixels"_a, "quality"_a = 90, "color_mode"_a = nb::none());
        m.def(
            "save_jpeg",
            [](const PixelArray& pixels, const std::filesystem::path& path, int quality, std::optional<ffpsd::ColorMode> color_mode) {
#if defined(FFPSD_HAS_JPEG)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                const std::string utf8 = ToUtf8(path);
                Saved([&] { ffpsd::SaveJpeg(view, utf8, quality); });
#else
                Lacks("JPEG");
#endif
            },
            "pixels"_a, "path"_a, "quality"_a = 90, "color_mode"_a = nb::none());

        m.def(
            "load_webp",
            [](const std::filesystem::path& path, ffpsd::ColorMode color_mode, std::uint16_t depth) -> NumpyArray {
#if defined(FFPSD_HAS_WEBP)
                const std::string utf8 = ToUtf8(path);
                return Decoded([&] { return ffpsd::LoadWebp(utf8, color_mode, depth); });
#else
                Lacks("WebP");
#endif
            },
            "path"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);
        m.def(
            "decode_webp",
            [](const BytesLike& data, ffpsd::ColorMode color_mode, std::uint16_t depth) -> NumpyArray {
#if defined(FFPSD_HAS_WEBP)
                return Decoded([&] { return ffpsd::LoadWebp(data.data(), data.size(), color_mode, depth); });
#else
                Lacks("WebP");
#endif
            },
            "data"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8);
        m.def(
            "encode_webp",
            [](const PixelArray& pixels, int quality, bool lossless, std::optional<ffpsd::ColorMode> color_mode) -> nb::bytes {
#if defined(FFPSD_HAS_WEBP)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                return Encoded([&] { return ffpsd::EncodeWebp(view, quality, lossless); });
#else
                Lacks("WebP");
#endif
            },
            "pixels"_a, "quality"_a = 90, "lossless"_a = false, "color_mode"_a = nb::none());
        m.def(
            "save_webp",
            [](const PixelArray& pixels, const std::filesystem::path& path, int quality, bool lossless,
               std::optional<ffpsd::ColorMode> color_mode) {
#if defined(FFPSD_HAS_WEBP)
                const ffpsd::ImageView view = ToView(pixels, PictureMode(pixels, color_mode));
                const std::string utf8 = ToUtf8(path);
                Saved([&] { ffpsd::SaveWebp(view, utf8, quality, lossless); });
#else
                Lacks("WebP");
#endif
            },
            "pixels"_a, "path"_a, "quality"_a = 90, "lossless"_a = false, "color_mode"_a = nb::none());
    }
} // namespace ffpsd_python
