// Opens a PSD, prints what it holds, copies the top layer and saves the result.
#include <cstddef>
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
        ffpsd::Document doc = ffpsd::Document::Parse(argv[1]);

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
