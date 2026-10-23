// Only how a picture finds its codec, not the codecs themselves.
#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

using namespace ffpsd_test;

namespace
{
    // What the build was configured with, which IsFormatSupported reports.
#if defined(FFPSD_HAS_PNG)
    constexpr bool kHasPng = true;
#else
    constexpr bool kHasPng = false;
#endif
#if defined(FFPSD_HAS_JPEG)
    constexpr bool kHasJpeg = true;
#else
    constexpr bool kHasJpeg = false;
#endif
#if defined(FFPSD_HAS_WEBP)
    constexpr bool kHasWebp = true;
#else
    constexpr bool kHasWebp = false;
#endif
} // namespace

TEST(FormatsTest, LoadPictureGoesByTheSignatureOfAFormatTheBuildHas)
{
    struct Case
    {
        std::string file;
        const char* misnamed;
        ffpsd::Format format;
        bool built;
        std::uint32_t width;
        std::uint32_t height;
        std::uint16_t channel_count;
    };

    // The first name is "picture" in Cyrillic: a path is UTF-8 whatever the system's code page.
    const Case cases[] = {
        {kRgbaPng, "ffpsd_\xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD0\xB8\xD0\xBD\xD0\xBA\xD0\xB0.jpg", ffpsd::Format::kPng, kHasPng, 3, 2, 4},
        {kQuadrantsJpeg, "ffpsd_picture.png", ffpsd::Format::kJpeg, kHasJpeg, 32, 16, 3},
        {kRgbaWebp, "ffpsd_picture.jpg", ffpsd::Format::kWebp, kHasWebp, 3, 2, 4},
    };
    for (const Case& c : cases)
    {
        EXPECT_EQ(ffpsd::IsFormatSupported(c.format), c.built) << c.file;

        const TempFile misnamed(c.misnamed);
        misnamed.Write(ReadFile(c.file));
        if (c.built)
        {
            const ffpsd::Image picture = ffpsd::LoadPicture(misnamed.Utf8(), ffpsd::ColorMode::kRgb, 8);
            EXPECT_EQ(picture.width, c.width) << c.file;
            EXPECT_EQ(picture.height, c.height) << c.file;
            EXPECT_EQ(picture.channel_count, c.channel_count) << c.file;
        }
        else
        {
            EXPECT_THROW(ffpsd::LoadPicture(misnamed.Utf8(), ffpsd::ColorMode::kRgb, 8), std::logic_error) << c.file;
        }
    }
}

TEST(FormatsTest, LoadPictureRefusesWhatIsNoPicture)
{
    const std::uint8_t psd[] = {'8', 'B', 'P', 'S', 0, 1};
    const std::uint8_t png_start[] = {0x89, 'P', 'N', 'G'};
    const std::uint8_t wave[] = {'R', 'I', 'F', 'F', 4, 0, 0, 0, 'W', 'A', 'V', 'E'};

    EXPECT_THROW(ffpsd::LoadPicture(psd, sizeof(psd), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(png_start, sizeof(png_start), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(wave, sizeof(wave), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(DataFile("no_such_file.png"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);
}
