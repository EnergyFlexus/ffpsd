#ifndef FFPSD_PYTHON_COMMON_HPP_
#define FFPSD_PYTHON_COMMON_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ffpsd_python
{
    namespace nb = nanobind;

    // A format this build lacks; Python sees UnsupportedError.
    struct Unsupported : std::logic_error
    {
        using std::logic_error::logic_error;
    };

    // Pixels as Python passes them: planar (channels, height, width), or (height, width) for one plane.
    using PixelArray = nb::ndarray<nb::ro, nb::c_contig, nb::device::cpu>;

    // What pixels come back as.
    using NumpyArray = nb::ndarray<nb::numpy>;

    // Any bytes-like object: bytes, bytearray, memoryview, a uint8 array.
    using ByteArray = nb::ndarray<const std::uint8_t, nb::ndim<1>, nb::c_contig, nb::device::cpu>;

    struct DocumentObject
    {
        ffpsd::Document document;

        // A serial per layer a handle was made for: a layer removed and another allocated at its address get different ones.
        std::unordered_map<const ffpsd::Layer*, std::uint64_t> serials;
        std::uint64_t next_serial = 1;

        explicit DocumentObject(ffpsd::Document&& value)
            : document(std::move(value))
        {
        }

        std::uint64_t Serial(const ffpsd::Layer* layer);

        // After a layer is destroyed: forgets every serial whose layer the document no longer has.
        void ForgetRemoved();

        // Python's index: negative counts from the top.
        std::size_t Index(std::int64_t index) const;
    };

    struct LayerObject
    {
        nb::object owner; // the Document, kept alive by every handle
        DocumentObject* document = nullptr;
        ffpsd::Layer* layer = nullptr;
        std::uint64_t serial = 0;

        // Throws std::logic_error once the layer is removed.
        ffpsd::Layer& Get() const;
    };

    LayerObject MakeLayer(DocumentObject& document, ffpsd::Layer* layer);

    // Paths are UTF-8 for ffpsd on every system.
    std::string ToUtf8(const std::filesystem::path& path);

    // Planar pixels as a new (channels, height, width) array that owns them.
    NumpyArray ToArray(ffpsd::Image&& image);

    // A view over the array's memory, which the caller keeps alive while ffpsd reads it.
    ffpsd::ImageView ToView(const PixelArray& pixels, ffpsd::ColorMode color_mode);
    ffpsd::ImageView ToView(const std::optional<PixelArray>& pixels, ffpsd::ColorMode color_mode);

    // Gray for one or two planes, RGB otherwise, unless the caller names the mode.
    ffpsd::ColorMode PictureMode(const PixelArray& pixels, std::optional<ffpsd::ColorMode> color_mode);

    nb::bytes ToBytes(const std::uint8_t* data, std::size_t size);

    void BindTypes(nb::module_& m);
    void BindDocument(nb::module_& m);
    void BindLayer(nb::module_& m);
    void BindFormats(nb::module_& m);
} // namespace ffpsd_python

#endif // FFPSD_PYTHON_COMMON_HPP_
