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
        image.color_mode = channels <= 2 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
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

TEST(FormatsJpegTest, LoadsTheColorsIntoPlanes)
{
    const ffpsd::Image rgb = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgb.width, 32u);
    EXPECT_EQ(rgb.height, 16u);
    EXPECT_EQ(rgb.channel_count, 3u);
    EXPECT_EQ(rgb.depth, 8u);
    const std::array<Color, 4> corners = Corners(rgb);
    EXPECT_TRUE(Near(corners[0], kRed));
    EXPECT_TRUE(Near(corners[1], kGreen));
    EXPECT_TRUE(Near(corners[2], kBlue));
    EXPECT_TRUE(Near(corners[3], kWhite));

    const ffpsd::Image progressive = ffpsd::LoadJpeg(Generated("rgb_quadrants_progressive.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(progressive.bytes.size(), rgb.bytes.size());
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_TRUE(Near(Corners(progressive)[i], corners[i])) << "progressive corner " << i;

    const ffpsd::Image gray = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_NEAR(At(gray, 2, 4), 64, kTolerance);
    EXPECT_NEAR(At(gray, 13, 4), 192, kTolerance);

    const ffpsd::Image cmyk = ffpsd::LoadJpeg(Generated("cmyk.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(cmyk.channel_count, 3u);
    EXPECT_TRUE(Near(RgbAt(cmyk, 2, 4), {0, 255, 255}));    // cyan
    EXPECT_TRUE(Near(RgbAt(cmyk, 13, 4), {127, 127, 127})); // half black
}

TEST(FormatsJpegTest, LoadsIntoTheModeAndDepthAskedFor)
{
    const ffpsd::Image gray = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 1u);
    EXPECT_EQ(gray.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_NEAR(At(gray, 2, 2), 54, kTolerance);    // 0.2126 of red
    EXPECT_NEAR(At(gray, 29, 2), 182, kTolerance);  // 0.7152 of green
    EXPECT_NEAR(At(gray, 2, 13), 18, kTolerance);   // 0.0722 of blue
    EXPECT_NEAR(At(gray, 29, 13), 255, kTolerance); // white

    const ffpsd::Image one = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kGrayscale, 8);
    const ffpsd::Image three = ffpsd::LoadJpeg(Generated("gray.jpg"), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(three.channel_count, 3u);
    EXPECT_EQ(three.color_mode, ffpsd::ColorMode::kRgb);
    for (std::uint16_t channel = 0; channel < 3; ++channel)
        EXPECT_EQ(At(three, 13, 4, channel), At(one, 13, 4));

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
    const Case cases[] = {{1, 32, 16, {kRed, kGreen, kBlue, kWhite}}, {2, 32, 16, {kGreen, kRed, kWhite, kBlue}},
                          {3, 32, 16, {kWhite, kBlue, kGreen, kRed}}, {4, 32, 16, {kBlue, kWhite, kRed, kGreen}},
                          {5, 16, 32, {kRed, kBlue, kGreen, kWhite}}, {6, 16, 32, {kBlue, kRed, kWhite, kGreen}},
                          {7, 16, 32, {kWhite, kGreen, kBlue, kRed}}, {8, 16, 32, {kGreen, kWhite, kRed, kBlue}}};
    for (const Case& expected : cases)
    {
        const ffpsd::Image image =
            ffpsd::LoadJpeg(Generated("orientation_" + std::to_string(expected.orientation) + ".jpg"), ffpsd::ColorMode::kRgb, 8);
        ASSERT_EQ(image.width, expected.width) << "orientation " << expected.orientation;
        ASSERT_EQ(image.height, expected.height) << "orientation " << expected.orientation;
        const std::array<Color, 4> corners = Corners(image);
        for (std::size_t i = 0; i < 4; ++i)
            EXPECT_TRUE(Near(corners[i], expected.corners[i])) << "orientation " << expected.orientation << ", corner " << i;
    }

    const ffpsd::Image little = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image big = ffpsd::LoadJpeg(Generated("orientation_6_big_endian.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(big.width, 16u);
    EXPECT_EQ(big.bytes, little.bytes);

    // Left as stored when asked, or when the EXIF is broken.
    const ffpsd::Image stored = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image unturned = ffpsd::LoadJpeg(Generated("orientation_6.jpg"), ffpsd::ColorMode::kRgb, 8, false);
    const ffpsd::Image broken = ffpsd::LoadJpeg(Generated("orientation_broken.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(unturned.width, 32u);
    EXPECT_EQ(unturned.bytes, stored.bytes);
    EXPECT_EQ(broken.width, 32u);
    EXPECT_EQ(broken.bytes, stored.bytes);
}

TEST(FormatsJpegTest, LoadingRefusesWhatItCannotDo)
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

TEST(FormatsJpegTest, EncodeThenLoadGivesTheColorsBack)
{
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}})
        {
            const std::uint16_t colors = channels >= 3 ? 3 : 1;
            const std::vector<std::uint8_t> jpeg = ffpsd::EncodeJpeg(Blocks(channels, depth), 100);
            const ffpsd::Image loaded =
                ffpsd::LoadJpeg(jpeg.data(), jpeg.size(), colors == 3 ? ffpsd::ColorMode::kRgb : ffpsd::ColorMode::kGrayscale, 8);

            ASSERT_EQ(loaded.channel_count, colors) << channels << " x " << depth;
            ASSERT_EQ(loaded.width, 16u);
            const ffpsd::Image expected = Blocks(colors, 8);
            for (std::size_t i = 0; i < expected.bytes.size(); ++i)
                ASSERT_NEAR(loaded.bytes[i], expected.bytes[i], kTolerance) << channels << " x " << depth << ", byte " << i;
        }
    }

    const ffpsd::Image image = ffpsd::LoadJpeg(Generated("rgb_quadrants.jpg"), ffpsd::ColorMode::kRgb, 8);
    EXPECT_LT(ffpsd::EncodeJpeg(image, 10).size(), ffpsd::EncodeJpeg(image, 100).size());

    const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / "ffpsd_image.jpg";
    ffpsd::SaveJpeg(image, path.string(), 80);
    EXPECT_EQ(ReadFile(path.string()), ffpsd::EncodeJpeg(image, 80));
    std::filesystem::remove(path);
}

TEST(FormatsJpegTest, EncodingRefusesWhatJpegCannotHold)
{
    ffpsd::Image cut = Pattern(2, 2, 3);
    cut.bytes.pop_back();

    EXPECT_THROW(ffpsd::EncodeJpeg(ffpsd::Image()), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3, 32)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 0), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3), 101), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(cut), std::invalid_argument);

    // Channels alone would pass for RGB with alpha and RGB.
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 4, 8, ffpsd::ColorMode::kCmyk)), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeJpeg(Pattern(2, 2, 3, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
}
