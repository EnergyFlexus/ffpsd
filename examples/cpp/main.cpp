// Opens a PSD, adds a copy of the top layer, a picture with a mask and a Levels layer, grows the canvas and saves the result.
#include <cstdint>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

// Every failure is an exception with a readable message.
void Run(const std::string& input, const std::string& picture_path, const std::string& output)
{
    ffpsd::Document doc = ffpsd::Document::Open(input);
    const std::uint32_t width = doc.GetWidth();
    const std::uint32_t height = doc.GetHeight();
    std::cout << width << " x " << height << ", " << doc.GetDepth() << " bit, " << doc.GetLayerCount() << " layers\n";

    // The composite Photoshop saved with the file, as PNG.
    const std::vector<std::uint8_t> png = ffpsd::EncodePng(doc.GetMergedImage());
    std::cout << "composite: " << png.size() << " bytes of PNG\n";

    // Layers go from the bottom up, so the top one is the last.
    const ffpsd::Layer& top = *doc.GetLayerByIndex(doc.GetLayerCount() - 1);
    const ffpsd::Rect bounds = top.GetBounds();
    std::cout << "top layer: " << top.GetName() << ", " << bounds.GetWidth() << " x " << bounds.GetHeight() << "\n";

    // A copy of it on top, half transparent, in Multiply mode.
    ffpsd::Layer* copy = doc.AddLayerCopy(top);
    copy->SetName("Copy");
    copy->SetOpacity(128);
    copy->SetBlendKey(0x6D756C20); // 'mul '

    // A PNG, JPEG or WebP at (50, 50), half the canvas, mirrored.
    const ffpsd::Image image = ffpsd::LoadPicture(picture_path, doc.GetColorMode(), doc.GetDepth());
    ffpsd::Layer* picture = doc.AddLayer("Picture", image, 50, 50);
    picture->Resize(width / 2, height / 2);
    picture->Flip(ffpsd::FlipDirection::kHorizontal);

    // A mask that hides a 100 x 100 square in its corner.
    const std::vector<std::uint8_t> hidden(100 * 100, 0);
    picture->SetMask(ffpsd::ImageView(100, 100, 1, 8, ffpsd::ColorMode::kGrayscale, hidden.data(), hidden.size()), 50, 50, 255);

    // A Levels layer over everything that brightens the midtones.
    ffpsd::LevelsInfo levels;
    levels.channels.resize(1);
    levels.channels[0].gamma = 1.5;
    doc.AddAdjustmentLayer("Levels", levels);

    // 100 px more canvas on every side.
    doc.ResizeCanvas(width + 200, height + 200, ffpsd::Anchor::kCenter);
    // Each channel RLE or raw, whichever is smaller.
    doc.Save(output, ffpsd::Compression::kRleOrRaw);
    std::cout << "saved " << doc.GetLayerCount() << " layers to " << output << "\n";
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: ffpsd_example_cpp <in.psd> <picture> <out.psd>\n";
        return 2;
    }

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); // layer names are UTF-8
#endif

    try
    {
        Run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
