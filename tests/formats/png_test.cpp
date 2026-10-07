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

TEST(FormatsPngTest, LoadsWhatTheFileHoldsIntoPlanes)
{
    const ffpsd::Image rgba = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgba.width, 3u);
    EXPECT_EQ(rgba.height, 2u);
    EXPECT_EQ(rgba.channel_count, 4u);
    EXPECT_EQ(rgba.depth, 8u);
    EXPECT_EQ(rgba.bytes, kRgbaPlanes);

    const ffpsd::Image gray = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kGrayscale, 16);
    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_EQ(Samples16(gray), (std::vector<std::uint16_t>{0x0000, 0xFFFF, 0x8080, 0x1234}));

    const ffpsd::Image palette = ffpsd::LoadPng(kPalettePng, ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(palette.channel_count, 4u);
    EXPECT_EQ(palette.bytes, (std::vector<std::uint8_t>{10, 200, 20, 100, 30, 50, 255, 0}));
}

TEST(FormatsPngTest, LoadsIntoTheModeAndDepthAskedFor)
{
    // 0.2126, 0.7152 and 0.0722 of 255, rounded.
    const ffpsd::Image gray = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(gray.bytes, (std::vector<std::uint8_t>{54, 182, 18, 255, 0, 128, 255, 128, 0, 255, 255, 64}));

    const ffpsd::Image wide = ffpsd::LoadPng(kRgbaPng, ffpsd::ColorMode::kRgb, 16);
    ASSERT_EQ(wide.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    EXPECT_EQ(samples[0], 65535u);
    EXPECT_EQ(samples[1], 0u);
    EXPECT_EQ(samples[5], 32896u); // 128 * 257

    const ffpsd::Image narrow = ffpsd::LoadPng(kGray16Png, ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(narrow.channel_count, 3u);
    EXPECT_EQ(narrow.color_mode, ffpsd::ColorMode::kRgb);
    const std::vector<std::uint8_t> plane = {0, 255, 128, 18};
    std::vector<std::uint8_t> expected;
    for (int i = 0; i < 3; ++i)
        expected.insert(expected.end(), plane.begin(), plane.end());
    EXPECT_EQ(narrow.bytes, expected);
}

TEST(FormatsPngTest, LoadingRefusesWhatItCannotDo)
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

TEST(FormatsPngTest, EncodeThenLoadGivesTheSamePlanes)
{
    // Gray, gray with alpha, RGB and RGBA are color types 0, 4, 2 and 6.
    const int color_types[] = {0, 4, 2, 6};
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}})
        {
            const ffpsd::Image image = Pattern(5, 3, channels, depth);
            const ffpsd::ColorMode color_mode = channels < 3 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;

            for (const ffpsd::PngCompression compression :
                 {ffpsd::PngCompression::kBalanced, ffpsd::PngCompression::kSmallest, ffpsd::PngCompression::kFastest})
            {
                const std::vector<std::uint8_t> png = ffpsd::EncodePng(image, compression);
                const int mode = static_cast<int>(compression);

                EXPECT_EQ(Header(png), std::make_tuple(5u, 3u, int{depth}, color_types[channels - 1])) << channels << " x " << depth;
                EXPECT_EQ(ffpsd::LoadPng(png.data(), png.size(), color_mode, depth).bytes, image.bytes)
                    << channels << " x " << depth << ", mode " << mode;
            }
        }
    }

    const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / "ffpsd_image.png";
    ffpsd::SavePng(Pattern(5, 3, 4), path.string(), ffpsd::PngCompression::kFastest);
    EXPECT_EQ(ReadFile(path.string()), ffpsd::EncodePng(Pattern(5, 3, 4), ffpsd::PngCompression::kFastest));
    std::filesystem::remove(path);
}

TEST(FormatsPngTest, EncodingRefusesWhatPngCannotHold)
{
    ffpsd::Image cut = Pattern(2, 2, 3);
    cut.bytes.pop_back();

    EXPECT_THROW(ffpsd::EncodePng(ffpsd::Image()), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 3, 32)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(cut), std::invalid_argument);

    // Channels alone would pass for RGBA and RGB.
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 4, 8, ffpsd::ColorMode::kCmyk)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodePng(Pattern(2, 2, 3, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
}
