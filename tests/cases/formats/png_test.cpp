// Conversions are worked out by hand.
#include "support/test_support.hpp"

#include <cstdint>
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
    const std::string kGray16Png = DataFile("generated/gray_16bit.png");
    const std::string kPalettePng = DataFile("generated/palette_transparent.png");

    // Width, height, bit depth and color type from the IHDR chunk, which always comes first.
    std::tuple<std::uint32_t, std::uint32_t, int, int> Header(const std::vector<std::uint8_t>& png)
    {
        return {BigEndianU32(png, 16), BigEndianU32(png, 20), png.at(24), png.at(25)};
    }

    ffpsd::Image Load(const std::vector<std::uint8_t>& png, ffpsd::ColorMode color_mode, std::uint16_t depth)
    {
        return ffpsd::LoadPng(png.data(), png.size(), color_mode, depth);
    }
} // namespace

TEST(FormatsPngTest, LoadsIntoPlanesOfTheModeAndDepthAskedFor)
{
    const ffpsd::Image rgba = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgba.width, 3u);
    EXPECT_EQ(rgba.height, 2u);
    EXPECT_EQ(rgba.channel_count, 4u);
    EXPECT_EQ(rgba.depth, 8u);
    EXPECT_EQ(rgba.bytes, kRgbaPlanes);

    const ffpsd::Image gray16 = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kGrayscale, 16);
    ASSERT_EQ(gray16.channel_count, 1u);
    EXPECT_EQ(Samples16(gray16), (std::vector<std::uint16_t>{0x0000, 0xFFFF, 0x8080, 0x1234}));

    const ffpsd::Image palette = ffpsd::LoadPng(kPalettePng, ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(palette.channel_count, 4u);
    EXPECT_EQ(palette.bytes, (ffpsd::Bytes{10, 200, 20, 100, 30, 50, 255, 0}));

    const ffpsd::Image gray = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(gray.bytes, kRgbaGrayPlanes);

    const ffpsd::Image wide = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 16);
    ASSERT_EQ(wide.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    EXPECT_EQ(samples[0], 65535u);
    EXPECT_EQ(samples[1], 0u);
    EXPECT_EQ(samples[5], 32896u); // 128 * 257

    const ffpsd::Image narrow = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(narrow.channel_count, 3u);
    EXPECT_EQ(narrow.color_mode, ffpsd::ColorMode::kRgb);
    EXPECT_EQ(narrow.bytes, (ffpsd::Bytes{0, 255, 128, 18, 0, 255, 128, 18, 0, 255, 128, 18}));
}

TEST(FormatsPngTest, RefusesWhatItCannotReadOrWrite)
{
    const std::vector<std::uint8_t> png = ReadFile(kRgbaPng);
    const std::vector<std::uint8_t> garbage(64, 0x42);
    const std::vector<std::uint8_t> cut(png.begin(), png.begin() + 40);

    EXPECT_THROW(Load(garbage, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(Load(cut, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(Load(png, ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(Load(png, ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadPng(DataFile("no_such_file.png"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);

    for (const ffpsd::Image& image : ImagesNoCodecTakes())
        EXPECT_THROW(ffpsd::EncodePng(image), std::invalid_argument) << image.channel_count << " x " << image.depth;
}

TEST(FormatsPngTest, EncodeThenLoadGivesTheSamePlanes)
{
    // Gray, gray with alpha, RGB and RGBA are color types 0, 4, 2 and 6.
    const int color_types[] = {0, 4, 2, 6};
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}})
        {
            const ffpsd::Image image = Pattern(5, 3, channels, depth);
            for (const ffpsd::PngCompression compression :
                 {ffpsd::PngCompression::kBalanced, ffpsd::PngCompression::kSmallest, ffpsd::PngCompression::kFastest})
            {
                const std::vector<std::uint8_t> png = ffpsd::EncodePng(image, compression);
                const int mode = static_cast<int>(compression);
                EXPECT_EQ(Header(png), std::make_tuple(5u, 3u, int{depth}, color_types[channels - 1])) << channels << " x " << depth;
                EXPECT_EQ(Load(png, image.color_mode, depth).bytes, image.bytes) << channels << " x " << depth << ", mode " << mode;
            }
        }
    }

    // Widths around the 16 pixel step of the vector loop, so it and the plain loop for the rest meet without a seam.
    for (const std::uint32_t width : {1u, 15u, 16u, 17u, 33u, 100u})
    {
        for (const std::uint16_t channels : {std::uint16_t{3}, std::uint16_t{4}})
        {
            const ffpsd::Image image = Pattern(width, 3, channels);
            EXPECT_EQ(Load(ffpsd::EncodePng(image), ffpsd::ColorMode::kRgb, 8).bytes, image.bytes) << width << " x " << channels;
        }
    }

    const TempFile file("ffpsd_image.png");
    ffpsd::SavePng(Pattern(5, 3, 4), file.Utf8(), ffpsd::PngCompression::kFastest);
    EXPECT_EQ(ReadFile(file.Path()), ffpsd::EncodePng(Pattern(5, 3, 4), ffpsd::PngCompression::kFastest));
}
