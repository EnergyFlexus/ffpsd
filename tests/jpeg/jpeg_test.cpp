// The JPEG files come from tests/scripts/make_jpeg_files.py; JPEG is lossy, so colors are compared within kTolerance.
#include "support/test_support.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
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
    constexpr int kTolerance = 4;

    using Color = std::array<int, 3>;
    constexpr Color kRed = {255, 0, 0};
    constexpr Color kGreen = {0, 255, 0};
    constexpr Color kBlue = {0, 0, 255};
    constexpr Color kWhite = {255, 255, 255};

    std::string Generated(const std::string& name)
    {
        return DataFile(("generated/" + name).c_str());
    }

    // The sample of an 8 bit image at x, y in a channel.
    int At(const ffpsd::Image& image, std::uint32_t x, std::uint32_t y, std::uint16_t channel = 0)
    {
        return image.bytes.at((std::size_t{channel} * image.height + y) * image.width + x);
    }

    Color RgbAt(const ffpsd::Image& image, std::uint32_t x, std::uint32_t y)
    {
        return {At(image, x, y, 0), At(image, x, y, 1), At(image, x, y, 2)};
    }

    ::testing::AssertionResult Near(const Color& actual, const Color& expected)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            if (std::abs(actual[i] - expected[i]) > kTolerance)
                return ::testing::AssertionFailure() << "got " << actual[0] << ", " << actual[1] << ", " << actual[2] << "; expected "
                                                     << expected[0] << ", " << expected[1] << ", " << expected[2];
        }
        return ::testing::AssertionSuccess();
    }

    // Top left, top right, bottom left and bottom right, away from the edges between the blocks.
    std::array<Color, 4> Corners(const ffpsd::Image& image)
    {
        const std::uint32_t right = image.width - 3;
        const std::uint32_t bottom = image.height - 3;
        return {RgbAt(image, 2, 2), RgbAt(image, right, 2), RgbAt(image, 2, bottom), RgbAt(image, right, bottom)};
    }

    // Flat blocks of 8 x 8, so the round trip stays within the tolerance.
    ffpsd::Image Blocks(std::uint16_t channels, std::uint16_t depth)
    {
        ffpsd::Image image;
        image.width = 16;
        image.height = 16;
        image.channel_count = channels;
        image.depth = depth;
        image.bytes.resize(image.GetSizeBytes());
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

TEST(JpegTest, LoadsRgbIntoPlanes)
{
    const ffpsd::Image image = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);

    EXPECT_EQ(image.width, 32u);
    EXPECT_EQ(image.height, 16u);
    EXPECT_EQ(image.channel_count, 3u);
    EXPECT_EQ(image.depth, 8u);
    const std::array<Color, 4> corners = Corners(image);
    EXPECT_TRUE(Near(corners[0], kRed));
    EXPECT_TRUE(Near(corners[1], kGreen));
    EXPECT_TRUE(Near(corners[2], kBlue));
    EXPECT_TRUE(Near(corners[3], kWhite));
}

TEST(JpegTest, AProgressiveFileLoadsTheSame)
{
    const ffpsd::Image baseline = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image progressive = ffpsd::LoadJpeg(Generated("rgb_quadrants_progressive.jpg"), ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(progressive.bytes.size(), baseline.bytes.size());
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_TRUE(Near(Corners(progressive)[i], Corners(baseline)[i]));
}

TEST(JpegTest, GrayLoadsAsOnePlaneOrThreeEqualOnes)
{
    const ffpsd::Image gray = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kGrayscale, 8);
    const ffpsd::Image rgb = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_NEAR(At(gray, 2, 4), 64, kTolerance);
    EXPECT_NEAR(At(gray, 13, 4), 192, kTolerance);

    ASSERT_EQ(rgb.channel_count, 3u);
    for (std::uint16_t channel = 0; channel < 3; ++channel)
        EXPECT_EQ(At(rgb, 13, 4, channel), At(gray, 13, 4));
}

TEST(JpegTest, RgbIntoGrayIsRec709Luma)
{
    const ffpsd::Image gray = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kGrayscale, 8);

    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_NEAR(At(gray, 2, 2), 54, kTolerance);    // 0.2126 of red
    EXPECT_NEAR(At(gray, 29, 2), 182, kTolerance);  // 0.7152 of green
    EXPECT_NEAR(At(gray, 2, 13), 18, kTolerance);   // 0.0722 of blue
    EXPECT_NEAR(At(gray, 29, 13), 255, kTolerance); // white
}

TEST(JpegTest, EightBitWidensToSixteenExactly)
{
    const ffpsd::Image narrow = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image wide = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 16);

    ASSERT_EQ(wide.depth, 16u);
    ASSERT_EQ(wide.bytes.size(), narrow.bytes.size() * 2);
    for (std::size_t i = 0; i < narrow.bytes.size(); ++i)
    {
        std::uint16_t sample = 0;
        std::memcpy(&sample, wide.bytes.data() + i * 2, 2);
        ASSERT_EQ(sample, narrow.bytes[i] * 257) << i;
    }
}

TEST(JpegTest, AdobeCmykBecomesRgb)
{
    const ffpsd::Image image = ffpsd::LoadJpeg(Generated("cmyk.jpg"), ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(image.channel_count, 3u);
    EXPECT_TRUE(Near(RgbAt(image, 2, 4), {0, 255, 255}));    // cyan
    EXPECT_TRUE(Near(RgbAt(image, 13, 4), {127, 127, 127})); // half black
}

struct OrientationCase
{
    int orientation;
    std::uint32_t width;
    std::uint32_t height;
    std::array<Color, 4> corners; // as Corners gives them
};

class JpegOrientationTest : public ::testing::TestWithParam<OrientationCase>
{
};

TEST_P(JpegOrientationTest, TurnsThePixelsUpright)
{
    const OrientationCase& expected = GetParam();
    const ffpsd::Image image =
        ffpsd::LoadJpeg(Generated("orientation_" + std::to_string(expected.orientation) + ".jpg"), ffpsd::ColorMode::kRgb, 8);

    ASSERT_EQ(image.width, expected.width);
    ASSERT_EQ(image.height, expected.height);
    const std::array<Color, 4> corners = Corners(image);
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_TRUE(Near(corners[i], expected.corners[i])) << "corner " << i;
}

// Stored: red top left, green top right, blue bottom left, white bottom right.
INSTANTIATE_TEST_SUITE_P(
    AllEight, JpegOrientationTest,
    ::testing::Values(
        OrientationCase{1, 32, 16, {kRed, kGreen, kBlue, kWhite}}, OrientationCase{2, 32, 16, {kGreen, kRed, kWhite, kBlue}},
        OrientationCase{3, 32, 16, {kWhite, kBlue, kGreen, kRed}}, OrientationCase{4, 32, 16, {kBlue, kWhite, kRed, kGreen}},
        OrientationCase{5, 16, 32, {kRed, kBlue, kGreen, kWhite}}, OrientationCase{6, 16, 32, {kBlue, kRed, kWhite, kGreen}},
        OrientationCase{7, 16, 32, {kWhite, kGreen, kBlue, kRed}}, OrientationCase{8, 16, 32, {kGreen, kWhite, kRed, kBlue}}));

TEST(JpegTest, BigEndianExifTurnsTheSame)
{
    const ffpsd::Image little = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image big = ffpsd::LoadJpeg(Generated("orientation_6_big_endian.jpg"), ffpsd::ColorMode::kRgb, 8);

    EXPECT_EQ(big.width, 16u);
    EXPECT_EQ(big.bytes, little.bytes);
}

TEST(JpegTest, OrientationCanBeLeftAsStored)
{
    const ffpsd::Image stored = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image unturned = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8, false);

    EXPECT_EQ(unturned.width, 32u);
    EXPECT_EQ(unturned.bytes, stored.bytes);
}

TEST(JpegTest, BrokenExifLoadsAsStored)
{
    const ffpsd::Image stored = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image broken = ffpsd::LoadJpeg(Generated("orientation_broken.jpg"), ffpsd::ColorMode::kRgb, 8);

    EXPECT_EQ(broken.width, 32u);
    EXPECT_EQ(broken.bytes, stored.bytes);
}

TEST(JpegTest, LoadingRefusesWhatItCannotDo)
{
    const std::vector<std::uint8_t> file = ReadFile(Generated("rgb_quadrants.jpg"));
    const std::vector<std::uint8_t> half(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(file.size() / 2));
    const std::vector<std::uint8_t> png = ReadFile(Generated("rgba_8bit.png"));

    EXPECT_THROW(ffpsd::LoadJpeg(file.data(), file.size(), ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadJpeg(file.data(), file.size(), ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadJpeg(png.data(), png.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadJpeg(half.data(), half.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadJpeg(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadJpeg(Generated("missing.jpg"), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
}

class JpegRoundTripTest : public ::testing::TestWithParam<std::tuple<std::uint16_t, std::uint16_t>>
{
};

TEST_P(JpegRoundTripTest, EncodeThenLoadGivesTheColorsBack)
{
    const auto [channels, depth] = GetParam();
    const ffpsd::Image image = Blocks(channels, depth);
    const std::uint16_t colors = channels >= 3 ? 3 : 1;

    const std::vector<std::uint8_t> jpeg = ffpsd::EncodeJpeg(image, 100);
    const ffpsd::Image loaded =
        ffpsd::LoadJpeg(jpeg.data(), jpeg.size(), colors == 3 ? ffpsd::ColorMode::kRgb : ffpsd::ColorMode::kGrayscale, 8);

    ASSERT_EQ(loaded.channel_count, colors);
    ASSERT_EQ(loaded.width, 16u);
    const ffpsd::Image expected = Blocks(colors, 8);
    for (std::size_t i = 0; i < expected.bytes.size(); ++i)
        ASSERT_NEAR(loaded.bytes[i], expected.bytes[i], kTolerance) << i;
}

INSTANTIATE_TEST_SUITE_P(
    ChannelsAndDepths, JpegRoundTripTest,
    ::testing::Combine(::testing::Values<std::uint16_t>(1, 2, 3, 4), ::testing::Values<std::uint16_t>(8, 16)));

TEST(JpegTest, LowerQualityMakesASmallerFile)
{
    const ffpsd::Image image = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);

    EXPECT_LT(ffpsd::EncodeJpeg(image, 10).size(), ffpsd::EncodeJpeg(image, 100).size());
}

TEST(JpegTest, EncodingRefusesWhatJpegCannotHold)
{
    ffpsd::Image cut = Pattern(2, 2, 3);
    cut.bytes.pop_back();

    EXPECT_THROW(ffpsd::EncodeJpeg(ffpsd::Image()), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3, 32)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 0), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 101), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(cut), std::invalid_argument);
}

TEST(JpegTest, ALayerSavesWithoutItsTransparency)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 16, 16);
    const ffpsd::Layer* layer = doc.AddLayer("blocks", Blocks(4, 8));

    const std::vector<std::uint8_t> jpeg = layer->EncodeJpeg(100);
    const ffpsd::Image loaded = ffpsd::LoadJpeg(jpeg.data(), jpeg.size(), ffpsd::ColorMode::kRgb, 8);

    const ffpsd::Image expected = Blocks(3, 8);
    ASSERT_EQ(loaded.bytes.size(), expected.bytes.size());
    for (std::size_t i = 0; i < expected.bytes.size(); ++i)
        ASSERT_NEAR(loaded.bytes[i], expected.bytes[i], kTolerance) << i;
}

TEST(JpegTest, SaveJpegWritesWhatEncodeJpegGives)
{
    const ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);
    const ffpsd::Layer* layer = doc.GetLayerByIndex(1);
    const std::filesystem::path layer_path = std::filesystem::path(testing::TempDir()) / "ffpsd_layer.jpg";
    const std::filesystem::path image_path = std::filesystem::path(testing::TempDir()) / "ffpsd_image.jpg";

    layer->SaveJpeg(layer_path.string(), 80);
    ffpsd::SaveJpeg(layer->GetPixels(), image_path.string(), 80);

    EXPECT_EQ(ReadFile(layer_path.string()), layer->EncodeJpeg(80));
    EXPECT_EQ(ReadFile(image_path.string()), ffpsd::EncodeJpeg(layer->GetPixels(), 80));
    std::filesystem::remove(layer_path);
    std::filesystem::remove(image_path);
}

TEST(JpegTest, OnlyAGrayOrRgbLayerWithPixelsSaves)
{
    const ffpsd::Document levels = ffpsd::Document::Open(kRgbLevelsPsd);
    ffpsd::Document cmyk = NewDocument(ffpsd::ColorMode::kCmyk);
    const ffpsd::Layer* cmyk_layer = cmyk.AddLayer("cmyk", Pattern(2, 2, 4));

    EXPECT_THROW(levels.GetLayerByIndex(1)->EncodeJpeg(), std::invalid_argument);
    EXPECT_THROW(cmyk_layer->EncodeJpeg(), std::invalid_argument);
}
