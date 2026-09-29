// Opens a PSD, prints what it holds, puts a copy of its top layer on top and saves the result.
#include <cstddef>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
    const char* ColorName(ffpsd::ColorMode color)
    {
        switch (color)
        {
        case ffpsd::ColorMode::kGrayscale:
            return "grayscale";
        case ffpsd::ColorMode::kRgb:
            return "RGB";
        case ffpsd::ColorMode::kCmyk:
            return "CMYK";
        case ffpsd::ColorMode::kLab:
            return "Lab";
        default:
            return "other";
        }
    }

    const char* KindName(ffpsd::LayerKind kind)
    {
        switch (kind)
        {
        case ffpsd::LayerKind::kRaster:
            return "raster";
        case ffpsd::LayerKind::kAdjustment:
            return "adjustment";
        case ffpsd::LayerKind::kGroupEnd:
            return "group end";
        default:
            return "group";
        }
    }

    bool IsGroupMarker(const ffpsd::Layer& layer)
    {
        const ffpsd::LayerKind kind = layer.GetKind();
        return kind != ffpsd::LayerKind::kRaster && kind != ffpsd::LayerKind::kAdjustment;
    }

    void PrintDocument(const ffpsd::Document& doc)
    {
        std::cout << doc.GetWidth() << " x " << doc.GetHeight() << ", " << ColorName(doc.GetColor()) << ", " << doc.GetDepth() << " bit"
                  << (doc.IsPsb() ? ", PSB" : "") << "\n";

        const ffpsd::ResolutionInfo resolution = doc.GetResolutionInfo();
        std::cout << "resolution: " << resolution.horizontal << " x " << resolution.vertical << " ppi\n";

        const ffpsd::VersionInfo version = doc.GetVersionInfo();
        std::cout << "written by: " << version.writer_name << ", for " << version.reader_name << "\n";

        std::cout << "layers, bottom to top: " << doc.GetLayerCount() << "\n";
        for (std::size_t i = 0; i < doc.GetLayerCount(); ++i)
        {
            const ffpsd::Layer* layer = doc.GetLayerByIndex(i);
            const ffpsd::Rect bounds = layer->GetBounds();
            std::cout << "  " << i << ": " << layer->GetName() << " (" << KindName(layer->GetKind()) << "), " << bounds.GetWidth() << " x "
                      << bounds.GetHeight() << " at " << bounds.left << ", " << bounds.top << (layer->IsVisible() ? "" : ", hidden")
                      << "\n";
        }
    }
} // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); // layer names are UTF-8
#endif
    if (argc != 3)
    {
        std::cerr << "usage: ffpsd_example_cpp <in.psd> <out.psd>\n";
        return 2;
    }

    try
    {
        ffpsd::Document doc = ffpsd::Document::Parse(argv[1]);
        PrintDocument(doc);

        // A copy of one group marker would break the nesting, so the top layer that is not one.
        const ffpsd::Layer* top = nullptr;
        for (std::size_t i = doc.GetLayerCount(); i-- > 0 && top == nullptr;)
        {
            if (!IsGroupMarker(*doc.GetLayerByIndex(i)))
                top = doc.GetLayerByIndex(i);
        }
        if (top == nullptr)
        {
            std::cerr << "no layer to copy\n";
            return 1;
        }

        doc.AddLayer(*top);
        doc.Save(argv[2]);
        std::cout << "\ncopied " << top->GetName() << " to the top, saved " << doc.GetLayerCount() << " layers to " << argv[2] << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
