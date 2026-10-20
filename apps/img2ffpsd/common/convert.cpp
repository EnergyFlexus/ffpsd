#include "common/convert.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <optional>
#include <thread>

namespace fs = std::filesystem;

namespace img2ffpsd
{
    namespace
    {
        // Every pixel of an 8 bit RGB picture has the same red, green and blue, whatever its file says.
        bool HasGrayPixels(const ffpsd::Image& image)
        {
            const std::size_t plane = std::size_t{image.width} * image.height;
            const std::uint8_t* red = image.bytes.data();
            const std::uint8_t* green = red + plane;
            const std::uint8_t* blue = green + plane;
            for (std::size_t i = 0; i < plane; ++i)
            {
                if (red[i] != green[i] || green[i] != blue[i])
                    return false;
            }
            return true;
        }

        // The red plane as the gray one, which is its luma exactly when the three are equal; transparency kept.
        ffpsd::Image ToGray(const ffpsd::Image& rgb)
        {
            ffpsd::ImageInfo info = rgb;
            info.channel_count = static_cast<std::uint16_t>(rgb.channel_count - 2);
            info.color_mode = ffpsd::ColorMode::kGrayscale;
            ffpsd::Image gray(info);

            const std::size_t plane = std::size_t{rgb.width} * rgb.height;
            std::memcpy(gray.bytes.data(), rgb.bytes.data(), plane);
            if (rgb.channel_count == 4)
                std::memcpy(gray.bytes.data() + plane, rgb.bytes.data() + 3 * plane, plane);
            return gray;
        }
    } // namespace

    std::string LayerName(std::size_t folder)
    {
        return folder == 0 ? "Background" : "Layer " + std::to_string(folder);
    }

    void Convert(const Conversion& conversion, const std::vector<Pictures>& found, const fs::path& name)
    {
        // The bottom and the top pictures, then those of the further folders that have the file.
        std::vector<fs::path> files;
        std::vector<std::size_t> folders; // the folder of each picture, which names its layer
        for (std::size_t i = 0; i < conversion.folders.size(); ++i)
        {
            const auto picture = found[i].find(name);
            if (picture != found[i].end())
            {
                files.push_back(conversion.folders[i] / picture->second);
                folders.push_back(i);
            }
        }

        // Auto takes the colors of the top picture, gray by its file or by its pixels; a top it had to decode is kept.
        bool gray = conversion.color == ColorChoice::kGray;
        std::optional<ffpsd::Image> top;
        if (conversion.color == ColorChoice::kAuto)
        {
            gray = IsGrayPicture(files[1]);
            if (!gray)
            {
                ffpsd::Image rgb = ffpsd::LoadPicture(files[1].u8string(), ffpsd::ColorMode::kRgb, 8);
                gray = HasGrayPixels(rgb);
                top = gray ? ToGray(rgb) : std::move(rgb);
            }
        }
        const ffpsd::ColorMode color_mode = gray ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        const std::uint16_t color_count = gray ? 1 : 3;

        std::vector<ffpsd::Image> images;
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            if (i == 1 && top.has_value())
                images.push_back(std::move(*top));
            else
                images.push_back(ffpsd::LoadPicture(files[i].u8string(), color_mode, 8));
        }

        // The document is the size and the colors of the top picture; every other one is resized and converted to them.
        const ffpsd::Image& bottom = images[0];
        const std::uint32_t width = images[1].width;
        const std::uint32_t height = images[1].height;

        ffpsd::Document doc(width, height, color_mode);

        if (bottom.width == width && bottom.height == height)
        {
            doc.AddBackgroundLayer(LayerName(0), bottom);
        }
        else
        {
            doc.AddLayer(LayerName(0), bottom)->Resize(width, height, conversion.filter);
            doc.SetBackgroundLayer(0);
        }

        // Every layer is opaque, so the upper one shown covers the rest and is the composite; the background when none is.
        const auto shown = [&](std::size_t i) { return folders[i] >= conversion.hidden.size() || !conversion.hidden[folders[i]]; };
        std::size_t composite = 0;
        for (std::size_t i = 1; i < images.size(); ++i)
        {
            if (shown(i))
                composite = i;
        }

        for (std::size_t i = 1; i < images.size(); ++i)
        {
            // A layer loses its transparency, the plane after the color ones.
            ffpsd::Image& image = images[i];
            image.channel_count = color_count;
            const ffpsd::ImageView colors(image, image.bytes.data(), image.GetSizeBytes());

            ffpsd::Layer* layer = doc.AddLayer(LayerName(folders[i]), colors);
            layer->SetVisible(shown(i));
            const bool resized = image.width != width || image.height != height;
            if (resized)
                layer->Resize(width, height, conversion.filter);

            // A resized layer is read back for the composite.
            if (i == composite)
            {
                if (resized)
                    doc.SetMergedImage(layer->GetPixels());
                else
                    doc.SetMergedImage(colors); // a ternary would copy the picture
            }
        }
        if (composite == 0)
            doc.SetMergedImage(doc.GetLayerByIndex(0)->GetPixels());

        fs::path target = conversion.output / name;
        target += ".psd"; // the name has no extension left to replace
        fs::create_directories(target.parent_path());
        doc.Save(target.u8string());
    }

    void ConvertAll(
        const Conversion& conversion, const std::vector<Pictures>& found, const std::vector<fs::path>& names, unsigned jobs,
        const FileDone& done, const std::atomic<bool>* cancel)
    {
        std::atomic<std::size_t> next{0};
        const auto work = [&] {
            for (std::size_t i = next++; i < names.size() && (cancel == nullptr || !*cancel); i = next++)
            {
                std::string error;
                try
                {
                    Convert(conversion, found, names[i]);
                }
                catch (const std::exception& e)
                {
                    error = e.what();
                }
                done(names[i], error);
            }
        };

        const unsigned processors = std::max(1u, std::thread::hardware_concurrency());
        const std::size_t threads = std::min<std::size_t>(jobs != 0 ? jobs : processors, names.size());
        std::vector<std::thread> workers;
        for (std::size_t i = 1; i < threads; ++i)
            workers.emplace_back(work);
        work(); // the calling thread is one of them
        for (std::thread& worker : workers)
            worker.join();
    }
} // namespace img2ffpsd
