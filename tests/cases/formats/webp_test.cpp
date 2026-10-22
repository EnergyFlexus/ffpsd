// The WebP files come from tests/scripts/make_webp_files.py; lossy colors are compared within kTolerance.
#include "support/test_support.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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

    const std::string kRgbaWebp = DataFile("generated/rgba_lossless.webp");
    const std::string kQuadrantsWebp = DataFile("generated/rgb_quadrants.webp");

    // Planes of rgba_lossless.webp: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
    const ffpsd::Bytes kRgbaPlanes = {255, 0,   0,   255, 0,   128, // R
                                      0,   255, 0,   255, 0,   128, // G
                                      0,   0,   255, 255, 0,   128, // B
                                      255, 128, 0,   255, 255, 64}; // A

    // Top left red, top right green, bottom left blue, bottom right white, away from the edges between them.
    void ExpectQuadrants(const ffpsd::Image& image)
    {
        ASSERT_EQ(image.width, 32u);
        ASSERT_EQ(image.height, 16u);
        ASSERT_EQ(image.channel_count, 3u);
        const std::array<std::array<std::uint32_t, 2>, 4> points = {{{2, 2}, {29, 2}, {2, 13}, {29, 13}}};
        const std::array<std::array<int, 3>, 4> colors = {{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}}};
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            for (std::uint16_t channel = 0; channel < 3; ++channel)
            {
                const int sample = image.bytes[(std::size_t{channel} * image.height + points[i][1]) * image.width + points[i][0]];
                EXPECT_NEAR(sample, colors[i][channel], kTolerance) << "corner " << i << ", channel " << channel;
            }
        }
    }
} // namespace

TEST(FormatsWebpTest, LoadsWhatTheFileHoldsIntoPlanes)
{
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgba.width, 3u);
    EXPECT_EQ(rgba.height, 2u);
    EXPECT_EQ(rgba.channel_count, 4u);
    EXPECT_EQ(rgba.depth, 8u);
    EXPECT_EQ(rgba.bytes, kRgbaPlanes);

    // A lossy file without alpha gives three planes.
    ExpectQuadrants(ffpsd::LoadWebp(kQuadrantsWebp, ffpsd::ColorMode::kRgb, 8));
}

TEST(FormatsWebpTest, LoadsIntoTheModeAndDepthAskedFor)
{
    // 0.2126, 0.7152 and 0.0722 of 255, rounded.
    const ffpsd::Image gray = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(gray.bytes, (ffpsd::Bytes{54, 182, 18, 255, 0, 128, 255, 128, 0, 255, 255, 64}));

    const ffpsd::Image wide = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 16);
    ASSERT_EQ(wide.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    ASSERT_EQ(samples.size(), kRgbaPlanes.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        EXPECT_EQ(samples[i], kRgbaPlanes[i] * 257) << i;
}

TEST(FormatsWebpTest, LoadingRefusesWhatItCannotDo)
{
    const std::vector<std::uint8_t> webp = ReadFile(kQuadrantsWebp);
    const std::vector<std::uint8_t> png = ReadFile(DataFile("generated/rgba_8bit.png"));
    const std::vector<std::uint8_t> animated = ReadFile(DataFile("generated/animated.webp"));
    std::vector<std::uint8_t> cut = webp;
    cut.resize(webp.size() / 2);

    EXPECT_THROW(ffpsd::LoadWebp(webp.data(), webp.size(), ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadWebp(webp.data(), webp.size(), ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(ffpsd::LoadWebp(png.data(), png.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(cut.data(), cut.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(animated.data(), animated.size(), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(DataFile("no_such_file.webp"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);
}

TEST(FormatsWebpTest, LosslessGivesTheSamePlanes)
{
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        const ffpsd::Image image = Pattern(5, 3, channels);
        const ffpsd::ColorMode color_mode = channels < 3 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        const std::vector<std::uint8_t> webp = ffpsd::EncodeWebp(image, 50, true);
        EXPECT_EQ(ffpsd::LoadWebp(webp.data(), webp.size(), color_mode, 8).bytes, image.bytes) << channels;
    }

    // An alpha opaque everywhere is left out, as WebP has no other way to hold it.
    const std::vector<std::uint8_t> opaque = ffpsd::EncodeWebp(Flat(4, 4, 4, 0xFF), 50, true);
    EXPECT_EQ(ffpsd::LoadWebp(opaque.data(), opaque.size(), ffpsd::ColorMode::kRgb, 8).channel_count, 3u);

    // The colors under a clear pixel stay too.
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    const std::vector<std::uint8_t> again = ffpsd::EncodeWebp(rgba, 100, true);
    EXPECT_EQ(ffpsd::LoadWebp(again.data(), again.size(), ffpsd::ColorMode::kRgb, 8).bytes, kRgbaPlanes);

    // 16 bit is narrowed to 8 as libpng narrows it.
    const ffpsd::Image wide = Pattern(5, 3, 3, 16);
    const std::vector<std::uint8_t> narrowed = ffpsd::EncodeWebp(wide, 50, true);
    const ffpsd::Image loaded = ffpsd::LoadWebp(narrowed.data(), narrowed.size(), ffpsd::ColorMode::kRgb, 8);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    ASSERT_EQ(loaded.bytes.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        ASSERT_EQ(loaded.bytes[i], (samples[i] * 255u + 32767) / 65535) << i;
}

TEST(FormatsWebpTest, LossyGivesTheColorsBack)
{
    const ffpsd::Image quadrants = ffpsd::LoadWebp(kQuadrantsWebp, ffpsd::ColorMode::kRgb, 8);
    const std::vector<std::uint8_t> webp = ffpsd::EncodeWebp(quadrants, 100);
    ExpectQuadrants(ffpsd::LoadWebp(webp.data(), webp.size(), ffpsd::ColorMode::kRgb, 8));
    EXPECT_LT(ffpsd::EncodeWebp(Pattern(64, 64, 3), 10).size(), ffpsd::EncodeWebp(Pattern(64, 64, 3), 100).size());

    // Alpha is kept whole, as libwebp keeps it by default.
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    const std::vector<std::uint8_t> lossy = ffpsd::EncodeWebp(rgba);
    const ffpsd::Image loaded = ffpsd::LoadWebp(lossy.data(), lossy.size(), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(loaded.channel_count, 4u);
    EXPECT_TRUE(std::equal(loaded.bytes.end() - 6, loaded.bytes.end(), kRgbaPlanes.end() - 6));

    const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / "ffpsd_image.webp";
    ffpsd::SaveWebp(quadrants, path.string(), 80);
    EXPECT_EQ(ReadFile(path.string()), ffpsd::EncodeWebp(quadrants, 80));
    std::filesystem::remove(path);
}

TEST(FormatsWebpTest, EncodingRefusesWhatWebpCannotHold)
{
    for (const ffpsd::Image& image : ImagesNoCodecTakes())
        EXPECT_THROW(ffpsd::EncodeWebp(image), std::invalid_argument) << image.channel_count << " x " << image.depth;
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(2, 2, 3), -1), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(2, 2, 3), 101), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(16384, 1, 3)), std::invalid_argument);
}
