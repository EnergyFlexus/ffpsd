// Lossy colors are compared within kTolerance.
#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

namespace
{
    ffpsd::Image Load(const std::vector<std::uint8_t>& webp, ffpsd::ColorMode color_mode, std::uint16_t depth)
    {
        return ffpsd::LoadWebp(webp.data(), webp.size(), color_mode, depth);
    }
} // namespace

TEST(FormatsWebpTest, LoadsIntoPlanesOfTheModeAndDepthAskedFor)
{
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(rgba.width, 3u);
    EXPECT_EQ(rgba.height, 2u);
    EXPECT_EQ(rgba.channel_count, 4u);
    EXPECT_EQ(rgba.depth, 8u);
    EXPECT_EQ(rgba.bytes, kRgbaPlanes);

    // A lossy file without alpha gives three planes.
    ExpectQuadrants(ffpsd::LoadWebp(kQuadrantsWebp, ffpsd::ColorMode::kRgb, 8));

    const ffpsd::Image gray = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kGrayscale, 8);
    ASSERT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.color_mode, ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(gray.bytes, kRgbaGrayPlanes);

    const ffpsd::Image wide = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 16);
    ASSERT_EQ(wide.depth, 16u);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    ASSERT_EQ(samples.size(), kRgbaPlanes.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        EXPECT_EQ(samples[i], kRgbaPlanes[i] * 257) << i;
}

TEST(FormatsWebpTest, RefusesWhatItCannotReadOrWrite)
{
    const std::vector<std::uint8_t> webp = ReadFile(kQuadrantsWebp);
    const std::vector<std::uint8_t> png = ReadFile(kRgbaPng);
    const std::vector<std::uint8_t> animated = ReadFile(DataFile("generated/animated.webp"));
    const std::vector<std::uint8_t> cut(webp.begin(), webp.begin() + static_cast<std::ptrdiff_t>(webp.size() / 2));

    EXPECT_THROW(Load(webp, ffpsd::ColorMode::kCmyk, 8), std::invalid_argument);
    EXPECT_THROW(Load(webp, ffpsd::ColorMode::kRgb, 32), std::invalid_argument);
    EXPECT_THROW(Load(png, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(Load(cut, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(Load(animated, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadWebp(DataFile("no_such_file.webp"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);

    for (const ffpsd::Image& image : ImagesNoCodecTakes())
        EXPECT_THROW(ffpsd::EncodeWebp(image), std::invalid_argument) << image.channel_count << " x " << image.depth;
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(2, 2, 3), -1), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(2, 2, 3), 101), std::invalid_argument);
    EXPECT_THROW(ffpsd::EncodeWebp(Pattern(16384, 1, 3)), std::invalid_argument);
}

TEST(FormatsWebpTest, LosslessGivesTheSamePlanes)
{
    for (const std::uint16_t channels : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{4}})
    {
        const ffpsd::Image image = Pattern(5, 3, channels);
        EXPECT_EQ(Load(ffpsd::EncodeWebp(image, 50, true), image.color_mode, 8).bytes, image.bytes) << channels;
    }

    // An alpha opaque everywhere is left out, as WebP has no other way to hold it.
    EXPECT_EQ(Load(ffpsd::EncodeWebp(Flat(4, 4, 4, 0xFF), 50, true), ffpsd::ColorMode::kRgb, 8).channel_count, 3u);

    // The colors under a clear pixel stay too.
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    EXPECT_EQ(Load(ffpsd::EncodeWebp(rgba, 100, true), ffpsd::ColorMode::kRgb, 8).bytes, kRgbaPlanes);

    // 16 bit is narrowed to 8 as libpng narrows it.
    const ffpsd::Image wide = Pattern(5, 3, 3, 16);
    const ffpsd::Image loaded = Load(ffpsd::EncodeWebp(wide, 50, true), ffpsd::ColorMode::kRgb, 8);
    const std::vector<std::uint16_t> samples = Samples16(wide);
    ASSERT_EQ(loaded.bytes.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        ASSERT_EQ(loaded.bytes[i], (samples[i] * 255u + 32767) / 65535) << i;
}

TEST(FormatsWebpTest, LossyGivesTheColorsBack)
{
    const ffpsd::Image quadrants = ffpsd::LoadWebp(kQuadrantsWebp, ffpsd::ColorMode::kRgb, 8);
    ExpectQuadrants(Load(ffpsd::EncodeWebp(quadrants, 100), ffpsd::ColorMode::kRgb, 8));
    EXPECT_LT(ffpsd::EncodeWebp(Pattern(64, 64, 3), 10).size(), ffpsd::EncodeWebp(Pattern(64, 64, 3), 100).size());

    // Alpha is kept whole, as libwebp keeps it by default.
    const ffpsd::Image rgba = ffpsd::LoadWebp(kRgbaWebp, ffpsd::ColorMode::kRgb, 8);
    const ffpsd::Image loaded = Load(ffpsd::EncodeWebp(rgba), ffpsd::ColorMode::kRgb, 8);
    ASSERT_EQ(loaded.channel_count, 4u);
    EXPECT_TRUE(std::equal(loaded.bytes.end() - 6, loaded.bytes.end(), kRgbaPlanes.end() - 6));

    const TempFile file("ffpsd_image.webp");
    ffpsd::SaveWebp(quadrants, file.Utf8(), 80);
    EXPECT_EQ(ReadFile(file.Path()), ffpsd::EncodeWebp(quadrants, 80));
}
