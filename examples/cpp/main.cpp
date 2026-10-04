// Opens a PSD, prints what it holds, copies the top layer, adds a gradient and saves the result.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: ffpsd_example_cpp <in.psd> <out.psd>\n";
        return 2;
    }

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); // layer names are UTF-8
#endif

    // Every failure is an exception with a readable message.
    try
    {
        // Open the document.
        ffpsd::Document doc = ffpsd::Document::Open(argv[1]);

        // Read what it holds; layers go from the bottom up.
        std::cout << doc.GetWidth() << " x " << doc.GetHeight() << ", " << doc.GetDepth() << " bit, " << doc.GetLayerCount() << " layers\n";
        for (std::size_t i = 0; i < doc.GetLayerCount(); ++i)
        {
            const ffpsd::Layer* layer = doc.GetLayerByIndex(i);
            const ffpsd::Rect bounds = layer->GetBounds();
            std::cout << "  " << i << ": " << layer->GetName() << ", " << bounds.GetWidth() << " x " << bounds.GetHeight() << "\n";
        }

        // Put a copy of the top layer on top; a group marker alone cannot be copied.
        const ffpsd::Layer* top = doc.GetLayerByIndex(doc.GetLayerCount() - 1);
        doc.AddLayerCopy(*top);

        // Add a gradient over the canvas, planar as PSD keeps it: red grows to the right, green down, blue stays at half.
        ffpsd::Image gradient;
        gradient.width = doc.GetWidth();
        gradient.height = doc.GetHeight();
        gradient.color_mode = doc.GetColorMode();
        gradient.channel_count = gradient.color_mode == ffpsd::ColorMode::kGrayscale ? 1 : 3;
        gradient.bytes.resize(gradient.GetSizeBytes());
        const std::size_t plane = std::size_t{gradient.width} * gradient.height;
        for (std::uint32_t y = 0; y < gradient.height; ++y)
        {
            for (std::uint32_t x = 0; x < gradient.width; ++x)
            {
                const std::size_t at = std::size_t{y} * gradient.width + x;
                gradient.bytes[at] = static_cast<std::uint8_t>(x * 255 / gradient.width);
                if (gradient.channel_count == 3)
                {
                    gradient.bytes[plane + at] = static_cast<std::uint8_t>(y * 255 / gradient.height);
                    gradient.bytes[2 * plane + at] = 128;
                }
            }
        }
        doc.AddLayer("Gradient", gradient);

        // Save, packed with RLE as Photoshop does.
        doc.Save(argv[2]);
        std::cout << "saved " << doc.GetLayerCount() << " layers to " << argv[2] << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
