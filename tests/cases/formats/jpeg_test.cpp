// JPEG is lossy, so colors are compared within kTolerance.
#include "support/test_support.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ffpsd_test;

namespace
{
    std::string Generated(const std::string& name)
    {
        return DataFile("generated/" + name);
    }

    ffpsd::Image Load(const std::vector<std::uint8_t>& jpeg, ffpsd::ColorMode color_mode, std::uint16_t depth)
    {
        return ffpsd::LoadJpeg(jpeg.data(), jpeg.size(), color_mode, depth);
    }

    // Flat blocks of 8 x 8, so the round trip stays within the tolerance.
    ffpsd::Image Blocks(std::uint16_t channels, std::uint16_t depth)
    {
        ffpsd::Image image = Pattern(16, 16, channels, depth);
        for (std::uint16_t channel = 0; channel < channels; ++channel)
        {
            for (std::uint32_t y = 0; y < 16; ++y)
            {
                for (std::uint32_t x = 0; x < 16; ++x)
                {
                    const unsigned value = 40u + 60u * channel + (x < 8 ? 0u : 50u) + (y < 8 ? 0u : 25u);
                    const std::size_t at = (std::size_t{channel} * 16 + y) * 16 + x;
                    if (depth == 8)
                    {
                        image.bytes[at] = static_cast<std::uint8_t>(value);
                    }
                    else
                    {
                        const auto sample = static_cast<std::uint16_t>(value * 257);
                        std::memcpy(image.bytes.data() + at * 2, &sample, 2);
                    }
                }
            }
        }
        return image;
    }
} // namespace

TEST(FormatsJpegTest, LoadsIntoPlanesOfTheModeAndDepthAskedFor)
{
    const ffpsd::Image rgb = ffpsd::LoadJpeg(kQuadrantsJpeg, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgb.depth, 8u);
    ExpectQuadrants(rgb);
    const ffpsd::Image progressive = ffpsd::LoadJpeg(Generated("rgb_quadrants_progressive.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(progressive.bytes.size(), rgb.bytes.size());
    ExpectCorners(progressive, Corners(rgb), "progressive");

    const ffpsd::Image gray = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_NEAR(SampleAt(gray, 2, 4), 64, kTolerance);
    EXPECT_NEAR(SampleAt(gray, 13, 4), 192, kTolerance);

    const ffpsd::Image cmyk = ffpsd::LoadJpeg(Generated("cmyk.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(cmyk.channel_count, 3u);
    EXPECT_TRUE(Near(RgbAt(cmyk, 2, 4), {0, 255, 255}));    // cyan
    EXPECT_TRUE(Near(RgbAt(cmyk, 13, 4), {127, 127, 127})); // half black

    const ffpsd::Image luma = ffpsd::LoadJpeg(kQuadrantsJpeg, ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(luma.channel_count, 1u);
    EXPECT_EQ(luma.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_NEAR(SampleAt(luma, 2, 2), 54, kTolerance);    // 0.2126 of red
    EXPECT_NEAR(SampleAt(luma, 29, 2), 182, kTolerance);  // 0.7152 of green
    EXPECT_NEAR(SampleAt(luma, 2, 13), 18, kTolerance);   // 0.0722 of blue
    EXPECT_NEAR(SampleAt(luma, 29, 13), 255, kTolerance); // white

    const ffpsd::Image three = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(three.channel_count, 3u);
    EXPECT_EQ(three.color_mode, ffpsd::ColorMode::kRgb);
    for (std::uint16_t channel = 0; channel < 3; ++channel)
        EXPECT_EQ(SampleAt(three, 13, 4, channel), SampleAt(gray, 13, 4));

    const ffpsd::Image wide = ffpsd::LoadJpeg(kQuadrantsJpeg, ffpsd::ColorMode::kRgb, 16);
    ASSERT_EQ(wide.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    ASSERT_EQ(samples.size(), rgb.bytes.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        ASSERT_EQ(samples[i], rgb.bytes[i] * 257) << i;
}

TEST(FormatsJpegTest, OrientationTurnsThePixelsUpright)
{
    struct Case
    {
        int orientation;
        std::uint32_t width;
        std::uint32_t height;
        std::array<Color, 4> corners; // as Corners gives them
    };
    // Stored: red top left, green top right, blue bottom left, white bottom right.
    const Case cases[] = {
        {1, 32, 16, {kRed, kGreen, kBlue, kWhite}}, {2, 32, 16, {kGreen, kRed, kWhite, kBlue}}, {3, 32, 16, {kWhite, kBlue, kGreen, kRed}},
        {4, 32, 16, {kBlue, kWhite, kRed, kGreen}}, {5, 16, 32, {kRed, kBlue, kGreen, kWhite}}, {6, 16, 32, {kBlue, kRed, kWhite, kGreen}},
        {7, 16, 32, {kWhite, kGreen, kBlue, kRed}}, {8, 16, 32, {kGreen, kWhite, kRed, kBlue}},
    };
    for (const Case& expected : cases)
    {
        const std::string name = "orientation_" + std::to_string(expected.orientation) + ".jpg";
        const ffpsd::Image image = ffpsd::LoadJpeg(Generated(name), ffpsd::ColorMode::kRgb, 8);
        ASSERT_EQ(image.width, expected.width) << name;
        ASSERT_EQ(image.height, expected.height) << name;
        ExpectCorners(image, expected.corners, name);
    }

    const ffpsd::Image little = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image big = ffpsd::LoadJpeg(Generated("orientation_6_big_endian.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(big.width, 16u);
    EXPECT_EQ(big.bytes, little.bytes);

    // Left as stored when asked, or when the EXIF is broken.
    const ffpsd::Image stored = ffpsd::LoadJpeg(kQuadrantsJpeg, ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image unturned = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8, false);
    const ffpsd::Image broken = ffpsd::LoadJpeg(Generated("orientation_broken.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(unturned.width, 32u);
    EXPECT_EQ(unturned.bytes, stored.bytes);
    EXPECT_EQ(broken.width, 32u);
    EXPECT_EQ(broken.bytes, stored.bytes);
}

TEST(FormatsJpegTest, RefusesWhatItCannotReadOrWrite)
{
    const std::vector<std::uint8_t> jpeg = ReadFile(kQuadrantsJpeg);
    const std::vector<std::uint8_t> half(jpeg.begin(), jpeg.begin() + static_cast<std::ptrdiff_t>(jpeg.size() / 2));
    const std::vector<std::uint8_t> png = ReadFile(kRgbaPng);

    EXPECT_THROW(Load(jpeg, ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(Load(jpeg, ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(Load(png, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(Load(half, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadJpeg(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadJpeg(Generated("missing.jpg"), ffpsd::ColorMode::kRgb, 8), std::runtime_error);

    for (const ffpsd::Image& image : ImagesNoCodecTakes())
        EXPECT_THROW(ffpsd::EncodeJpeg(image), std::invalid_argument) << image.channel_count << " x " << image.depth;
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 0), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 101), std::invalid_argument);
}

TEST(FormatsJpegTest, EncodeThenLoadGivesTheColorsBack)
{
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}})
        {
            const std::uint16_t colors = channels >= 3 ? 3 : 1;
            const ffpsd::Image expected = Blocks(colors, 8);
            const ffpsd::Image loaded = Load(ffpsd::EncodeJpeg(Blocks(channels, depth), 100), expected.color_mode, 8);

            ASSERT_EQ(loaded.channel_count, colors) << channels << " x " << depth;
            ASSERT_EQ(loaded.width, 16u);
            for (std::size_t i = 0; i < expected.bytes.size(); ++i)
                ASSERT_NEAR(loaded.bytes[i], expected.bytes[i], kTolerance) << channels << " x " << depth << ", byte " << i;
        }
    }

    const ffpsd::Image image = ffpsd::LoadJpeg(kQuadrantsJpeg, ffpsd::ColorMode::kRgb, 8);
    EXPECT_LT(ffpsd::EncodeJpeg(image, 10).size(), ffpsd::EncodeJpeg(image, 100).size());

    const TempFile file("ffpsd_image.jpg");
    ffpsd::SaveJpeg(image, file.Utf8(), 80);
    EXPECT_EQ(ReadFile(file.Path()), ffpsd::EncodeJpeg(image, 80));
}
