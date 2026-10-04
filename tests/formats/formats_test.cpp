// The codecs themselves are checked in png_test.cpp and jpeg_test.cpp; here only what picks them.
#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(FormatsTest, IsFormatSupportedFollowsTheBuild)
{
#if defined(FFPSD_HAS_PNG)
    EXPECT_TRUE(ffpsd::IsFormatSupported(ffpsd::Format::kPng));
#else
    EXPECT_FALSE(ffpsd::IsFormatSupported(ffpsd::Format::kPng));
#endif
#if defined(FFPSD_HAS_JPEG)
    EXPECT_TRUE(ffpsd::IsFormatSupported(ffpsd::Format::kJpeg));
#else
    EXPECT_FALSE(ffpsd::IsFormatSupported(ffpsd::Format::kJpeg));
#endif
}

TEST(FormatsTest, LoadPictureGoesByTheSignatureNotTheName)
{
    struct Case
    {
        const char* file;
        const char* misnamed;
        ffpsd::Format format;
        std::uint32_t width;
        std::uint32_t height;
        std::uint16_t channel_count;
    };
    const Case cases[] = {
        {"generated/rgba_8bit.png", "ffpsd_\xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD0\xB8\xD0\xBD\xD0\xBA\xD0\xB0.jpg", ffpsd::Format::kPng, 3, 2,
         4},
        {"generated/rgb_quadrants.jpg", "ffpsd_picture.png", ffpsd::Format::kJpeg, 32, 16, 3}};

    for (const Case& c : cases)
    {
        const std::vector<std::uint8_t> data = ReadFile(DataFile(c.file));
        // The first name is "picture" in Cyrillic: a path is UTF-8 whatever the system's code page.
        const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / std::filesystem::u8path(c.misnamed);
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

        if (ffpsd::IsFormatSupported(c.format))
        {
            const ffpsd::Image picture = ffpsd::LoadPicture(path.u8string(), ffpsd::ColorMode::kRgb, 8);
            EXPECT_EQ(picture.width, c.width) << c.file;
            EXPECT_EQ(picture.height, c.height) << c.file;
            EXPECT_EQ(picture.channel_count, c.channel_count) << c.file;
        }
        else
        {
            EXPECT_THROW(ffpsd::LoadPicture(path.u8string(), ffpsd::ColorMode::kRgb, 8), std::logic_error) << c.file;
        }
        std::filesystem::remove(path);
    }
}

TEST(FormatsTest, LoadPictureRefusesAnUnsupportedFormat)
{
    const std::uint8_t psd[] = {'8', 'B', 'P', 'S', 0, 1};
    const std::uint8_t png_start[] = {0x89, 'P', 'N', 'G'};

    EXPECT_THROW(ffpsd::LoadPicture(psd, sizeof(psd), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(png_start, sizeof(png_start), ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(nullptr, 0, ffpsd::ColorMode::kRgb, 8), std::runtime_error);
    EXPECT_THROW(ffpsd::LoadPicture(DataFile("no_such_file.png"), ffpsd::ColorMode::kRgb, 8), std::filesystem::filesystem_error);
}
