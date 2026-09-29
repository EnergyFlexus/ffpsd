// The PNG files come from tests/scripts/make_png_files.py; conversions are worked out by hand.
#include "support/test_support.hpp"

#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace ffpsd_test;

namespace
{
    const std::string kRgbaPng = DataFile("generated/rgba_8bit.png");
    const std::string kGray16Png = DataFile("generated/gray_16bit.png");
    const std::string kPalettePng = DataFile("generated/palette_transparent.png");

    // Planes of rgba_8bit.png: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
    const std::vector<std::uint8_t> kRgbaPlanes = {255, 0,   0,   255, 0,   128, // R
                                                   0,   255, 0,   255, 0,   128, // G
                                                   0,   0,   255, 255, 0,   128, // B
                                                   255, 128, 0,   255, 255, 64}; // A

    std::vector<std::uint16_t> Samples16(const ffpsd::Image& image)
    {
        std::vector<std::uint16_t> samples(image.bytes.size() / 2);
        std::memcpy(samples.data(), image.bytes.data(), image.bytes.size());
        return samples;
    }

    // Width, height, bit depth and color type from the IHDR chunk, which always comes first.
    std::tuple<std::uint32_t, std::uint32_t, int, int> Header(const std::vector<std::uint8_t>& png)
    {
        const std::vector<std::uint8_t> width(png.begin() + 16, png.begin() + 20);
        const std::vector<std::uint8_t> height(png.begin() + 20, png.begin() + 24);
        return {BigEndianU32(width), BigEndianU32(height), png.at(24), png.at(25)};
    }
} // namespace

TEST(PngTest, LoadsRgbaIntoPlanes)
{
    const ffpsd::Image image = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 8);

    EXPECT_EQ(image.width, 3u);
    EXPECT_EQ(image.height, 2u);
    EXPECT_EQ(image.channel_count, 4u);
    EXPECT_EQ(image.depth, 8u);
    EXPECT_EQ(image.bytes, kRgbaPlanes);
}

TEST(PngTest, RgbIntoGrayIsRec709Luma)
{
    const ffpsd::Image image = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kGrayscale, 8);

    ASSERT_EQ(image.channel_count, 2u);
    const std::vector<std::uint8_t> expected = {54,  182, 18, 255, 0,   128, // 0.2126, 0.7152 and 0.0722 of 255, rounded
                                                255, 128, 0,  255, 255, 64};
    EXPECT_EQ(image.bytes, expected);
}

TEST(PngTest, EightBitWidensToSixteenExactly)
{
    const ffpsd::Image image = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 16);

    ASSERT_EQ(image.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(image);
    EXPECT_EQ(samples[0], 65535u);
    EXPECT_EQ(samples[1], 0u);
    EXPECT_EQ(samples[5], 32896u); // 128 * 257
}

TEST(PngTest, SixteenBitGrayKeepsItsSamples)
{
    const ffpsd::Image image = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kGrayscale, 16);

    ASSERT_EQ(image.channel_count, 1u);
    EXPECT_EQ(Samples16(image), (std::vector<std::uint16_t>{0x0000, 0xFFFF, 0x8080, 0x1234}));
}

TEST(PngTest, SixteenBitNarrowsToEightRounded)
{
    const ffpsd::Image image = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(image.channel_count, 3u);
    const std::vector<std::uint8_t> plane = {0, 255, 128, 18};
    std::vector<std::uint8_t> expected;
    for (int i = 0; i < 3; ++i)
        expected.insert(expected.end(), plane.begin(), plane.end());
    EXPECT_EQ(image.bytes, expected);
}

TEST(PngTest, APaletteWithTransparencyBecomesRgba)
{
    const ffpsd::Image image = ffpsd::LoadPng(kPalettePng, ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(image.channel_count, 4u);
    EXPECT_EQ(image.bytes, (std::vector<std::uint8_t>{10, 200, 20, 100, 30, 50, 255, 0}));
}

TEST(PngTest, LoadingRefusesWhatItCannotDo)
{
    const std::vector<std::uint8_t> png = ReadFile(kRgbaPng);
    const std::vector<std::uint8_t> garbage(64, 0x42);
    std::vector<std::uint8_t> cut = png;
    cut.resize(40);

    EXPECT_THROW(ffpsd::LoadPng(garbage.data(), garbage.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPng(cut.data(), cut.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPng(png.data(), png.size(), ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadPng(png.data(), png.size(), ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadPng(DataFile("no_such_file.png"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);
}

class PngRoundTripTest : public testing::TestWithParam<std::tuple<std::uint16_t, std::uint16_t>>
{
};

TEST_P(PngRoundTripTest, EncodeThenLoadGivesTheSamePlanes)
{
    const auto [channels, depth] = GetParam();
    const ffpsd::Image image = Pattern(5, 3, channels, depth);
    const ffpsd::ColorMode color = channels < 3 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;

    const std::vector<std::uint8_t> png = ffpsd::EncodePng(image);

    // Gray, gray with alpha, RGB and RGBA are color types 0, 4, 2 and 6.
    const int color_types[] = {0, 4, 2, 6};
    EXPECT_EQ(Header(png), std::make_tuple(5u, 3u, int{depth}, color_types[channels - 1]));
    EXPECT_EQ(ffpsd::LoadPng(png.data(), png.size(), color, depth).bytes, image.bytes);
}

INSTANTIATE_TEST_SUITE_P(
    ChannelsAndDepths, PngRoundTripTest,
    testing::Combine(testing::Values<std::uint16_t>(1, 2, 3, 4), testing::Values<std::uint16_t>(8, 16)));

TEST(PngTest, EncodingRefusesWhatPngCannotHold)
{
    ffpsd::Image cut = Pattern(2, 2, 3);
    cut.bytes.pop_back();

    EXPECT_THROW(ffpsd::EncodePng(ffpsd::Image()), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 3, 32)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(cut), std::invalid_argument);
}

TEST(PngTest, LayersOfBothFilesSaveAsWhatGetPixelsGives)
{
    const ffpsd::Document rgb = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Document gray = ffpsd::Document::Parse(kGrayscalePsd);

    for (const auto& [doc, color] : {std::make_pair(&rgb, ffpsd::ColorMode::kRgb), std::make_pair(&gray, ffpsd::ColorMode::kGrayscale)})
    {
        for (std::size_t i = 0; i < doc->GetLayerCount(); ++i)
        {
            const ffpsd::Layer* layer = doc->GetLayerByIndex(i);
            const std::vector<std::uint8_t> png = layer->SaveAsPng();
            EXPECT_EQ(ffpsd::LoadPng(png.data(), png.size(), color, 8).bytes, layer->GetPixels().bytes) << layer->GetName();
        }
    }
}

TEST(PngTest, ASixteenBitLayerSavesSixteenBit)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 16);
    const ffpsd::Layer* layer = doc.AddLayer("deep", Pattern(3, 2, 4, 16));

    const std::vector<std::uint8_t> png = layer->SaveAsPng();

    EXPECT_EQ(std::get<2>(Header(png)), 16);
    EXPECT_EQ(ffpsd::LoadPng(png.data(), png.size(), ffpsd::ColorMode::kRgb, 16).bytes, Pattern(3, 2, 4, 16).bytes);
}

TEST(PngTest, SaveAsPngWritesTheSameBytesToAFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / "ffpsd_layer.png";

    doc.GetLayerByIndex(1)->SaveAsPng(path.string());

    EXPECT_EQ(ReadFile(path.string()), doc.GetLayerByIndex(1)->SaveAsPng());
    std::filesystem::remove(path);
}

TEST(PngTest, OnlyAGrayOrRgbLayerWithPixelsSaves)
{
    const ffpsd::Document levels = ffpsd::Document::Parse(kRgbLevelsPsd);
    ffpsd::Document cmyk = NewDocument(ffpsd::ColorMode::kCmyk);
    const ffpsd::Layer* cmyk_layer = cmyk.AddLayer("cmyk", Pattern(2, 2, 4));

    EXPECT_THROW(levels.GetLayerByIndex(1)->SaveAsPng(), std::invalid_argument);
    EXPECT_THROW(cmyk_layer->SaveAsPng(), std::invalid_argument);
}
