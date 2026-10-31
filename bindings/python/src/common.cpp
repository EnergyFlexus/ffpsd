#include "common.hpp"

#include <limits>
#include <unordered_set>
#include <utility>

namespace ffpsd_python
{
    std::uint64_t DocumentObject::Serial(const ffpsd::Layer* layer)
    {
        const auto [at, added] = serials.try_emplace(layer, next_serial);
        if (added)
            ++next_serial;
        return at->second;
    }

    void DocumentObject::ForgetRemoved()
    {
        std::unordered_set<const ffpsd::Layer*> alive;
        for (std::size_t i = 0; i < document.GetLayerCount(); ++i)
            alive.insert(document.GetLayerByIndex(i));
        for (auto at = serials.begin(); at != serials.end();)
            at = alive.count(at->first) ? std::next(at) : serials.erase(at);
    }

    std::size_t DocumentObject::Index(std::int64_t index) const
    {
        const auto count = static_cast<std::int64_t>(document.GetLayerCount());
        const std::int64_t at = index < 0 ? index + count : index;
        if (at < 0 || at >= count)
            throw std::out_of_range("ffpsd: layer index " + std::to_string(index) + " of " + std::to_string(count));
        return static_cast<std::size_t>(at);
    }

    ffpsd::Layer& LayerObject::Get() const
    {
        const auto at = document->serials.find(layer);
        if (at == document->serials.end() || at->second != serial)
            throw std::logic_error("ffpsd: the layer was removed from its document");
        return *layer;
    }

    LayerObject MakeLayer(DocumentObject& document, ffpsd::Layer* layer)
    {
        LayerObject result;
        result.owner = nb::find(document);
        result.document = &document;
        result.layer = layer;
        result.serial = document.Serial(layer);
        return result;
    }

    std::string ToUtf8(const std::filesystem::path& path)
    {
        // u8string is std::string in C++17 and std::u8string in C++20.
        const auto text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    namespace
    {
        nb::dlpack::dtype DtypeOf(std::uint16_t depth)
        {
            switch (depth)
            {
            case 8:
                return nb::dtype<std::uint8_t>();
            case 16:
                return nb::dtype<std::uint16_t>();
            case 32:
                return nb::dtype<float>();
            default:
                throw std::invalid_argument("ffpsd: no array type for depth " + std::to_string(depth));
            }
        }

        std::uint16_t DepthOf(nb::dlpack::dtype dtype)
        {
            if (dtype == nb::dtype<std::uint8_t>())
                return 8;
            if (dtype == nb::dtype<std::uint16_t>())
                return 16;
            if (dtype == nb::dtype<float>())
                return 32;
            throw std::invalid_argument("ffpsd: pixels are uint8, uint16 or float32");
        }

        template <class T> T Fit(std::size_t value, const char* what)
        {
            if (value > std::numeric_limits<T>::max())
                throw std::invalid_argument(std::string("ffpsd: too many ") + what + ": " + std::to_string(value));
            return static_cast<T>(value);
        }
    } // namespace

    NumpyArray ToArray(ffpsd::Image&& image)
    {
        const nb::dlpack::dtype dtype = DtypeOf(image.depth);
        const std::size_t shape[3] = {image.channel_count, image.height, image.width};
        auto* owned = new ffpsd::Image(std::move(image));
        nb::capsule owner(owned, [](void* pointer) noexcept { delete static_cast<ffpsd::Image*>(pointer); });

        // A zero-size array still needs a pointer.
        static std::uint8_t empty = 0;
        void* data = owned->bytes.empty() ? &empty : owned->bytes.data();
        return NumpyArray(data, 3, shape, owner, nullptr, dtype);
    }

    ffpsd::ImageView ToView(const PixelArray& pixels, ffpsd::ColorMode color_mode)
    {
        if (pixels.ndim() != 2 && pixels.ndim() != 3)
            throw std::invalid_argument(
                "ffpsd: pixels are (channels, height, width) or (height, width), not " + std::to_string(pixels.ndim()) + " dimensions");
        const bool planes = pixels.ndim() == 3;
        ffpsd::ImageView view;
        view.channel_count = planes ? Fit<std::uint16_t>(pixels.shape(0), "channels") : 1;
        view.height = Fit<std::uint32_t>(pixels.shape(planes ? 1 : 0), "rows");
        view.width = Fit<std::uint32_t>(pixels.shape(planes ? 2 : 1), "columns");
        view.depth = DepthOf(pixels.dtype());
        view.color_mode = color_mode;
        view.data = static_cast<const std::uint8_t*>(pixels.data());
        view.size = pixels.nbytes();
        return view;
    }

    ffpsd::ImageView ToView(const std::optional<PixelArray>& pixels, ffpsd::ColorMode color_mode)
    {
        return pixels ? ToView(*pixels, color_mode) : ffpsd::ImageView();
    }

    ffpsd::ColorMode PictureMode(const PixelArray& pixels, std::optional<ffpsd::ColorMode> color_mode)
    {
        if (color_mode)
            return *color_mode;
        const std::size_t channels = pixels.ndim() == 3 ? pixels.shape(0) : 1;
        return channels <= 2 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
    }

    nb::bytes ToBytes(const std::uint8_t* data, std::size_t size)
    {
        return nb::bytes(reinterpret_cast<const char*>(data), size);
    }
} // namespace ffpsd_python
