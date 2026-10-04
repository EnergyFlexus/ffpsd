#include "support/test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

namespace
{
    // Rec. 709 luma in 15 bit fixed point: 0.2126, 0.7152 and 0.0722 times 32768, rounded.
    std::uint8_t Luma(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
    {
        return static_cast<std::uint8_t>((6966u * red + 23436u * green + 2366u * blue + 16384u) >> 15);
    }

    // The gray planes an 8 bit RGB image should become: luma, then every plane after blue as it is.
    std::vector<std::uint8_t> ExpectedGray(const ffpsd::Image& rgb)
    {
        const std::size_t plane = std::size_t{rgb.width} * rgb.height;
        std::vector<std::uint8_t> gray(plane * (rgb.channel_count - 2u));
        for (std::size_t i = 0; i < plane; ++i)
            gray[i] = Luma(rgb.bytes[i], rgb.bytes[plane + i], rgb.bytes[2 * plane + i]);
        std::copy(rgb.bytes.begin() + 3 * plane, rgb.bytes.end(), gray.begin() + plane);
        return gray;
    }

    // The RGB planes a gray image should become: the gray plane three times, then every plane after it.
    std::vector<std::uint8_t> ExpectedRgb(const ffpsd::Image& gray)
    {
        const std::size_t plane = std::size_t{gray.width} * gray.height;
        std::vector<std::uint8_t> rgb;
        for (int i = 0; i < 3; ++i)
            rgb.insert(rgb.end(), gray.bytes.begin(), gray.bytes.begin() + plane);
        rgb.insert(rgb.end(), gray.bytes.begin() + plane, gray.bytes.end());
        return rgb;
    }

    void ExpectRecord(const ffpsd::LevelsInfo::Channel& record, std::uint16_t floor, std::uint16_t ceiling, const char* what)
    {
        EXPECT_EQ(record.input_floor, floor) << what;
        EXPECT_EQ(record.input_ceiling, ceiling) << what;
        EXPECT_EQ(record.output_floor, 0u) << what;
        EXPECT_EQ(record.output_ceiling, 255u) << what;
        EXPECT_DOUBLE_EQ(record.gamma, 1.0) << what;
    }
} // namespace

TEST(DocumentColorModeTest, OnlyRgbAndGrayConvertAndARefusalChangesNothing)
{
    ffpsd::Document rgb = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Document cmyk = NewDocument(ffpsd::ColorMode::kCmyk);
    const std::vector<std::uint8_t> before = rgb.Save();

    rgb.ConvertColorMode(ffpsd::ColorMode::kRgb);
    EXPECT_EQ(rgb.Save(), before);

    EXPECT_THROW(rgb.ConvertColorMode(ffpsd::ColorMode::kCmyk), std::invalid_argument);
    EXPECT_THROW(rgb.ConvertColorMode(ffpsd::ColorMode::kLab), std::invalid_argument);
    EXPECT_THROW(cmyk.ConvertColorMode(ffpsd::ColorMode::kGrayscale), std::invalid_argument);
    EXPECT_EQ(rgb.Save(), before);
    EXPECT_EQ(cmyk.GetColorMode(), ffpsd::ColorMode::kCmyk);

    // An adjustment it cannot convert stops it before anything changes.
    rgb.GetLayerByIndex(1)->SetTaggedBlock(Block("curv", {0, 1}));
    const std::vector<std::uint8_t> with_curves = rgb.Save();
    EXPECT_THROW(rgb.ConvertColorMode(ffpsd::ColorMode::kGrayscale), std::invalid_argument);
    EXPECT_EQ(rgb.GetColorMode(), ffpsd::ColorMode::kRgb);
    EXPECT_EQ(rgb.Save(), with_curves);
}

TEST(DocumentColorModeTest, RgbToGrayIsLumaWithTransparencyKept)
{
    // Red, green, blue and white at four alphas; the composite is the same colors, opaque.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 4, 1);
    ffpsd::Image layer;
    layer.width = 4;
    layer.height = 1;
    layer.channel_count = 4;
    layer.bytes = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 128, 0, 64};
    doc.AddLayer("colors", layer);
    ffpsd::Image composite = layer;
    composite.channel_count = 3;
    composite.bytes.resize(12);
    doc.SetMergedImage(composite);
    ffpsd::ImageResource profile;
    profile.id = 1039;
    profile.data = {1, 2, 3};
    doc.SetImageResource(profile);

    doc.ConvertColorMode(ffpsd::ColorMode::kGrayscale);

    // 6966 * 255 >> 15 is 54, 23436 * 255 >> 15 is 182, 2366 * 255 >> 15 is 18; white stays 255.
    EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(doc.GetChannelCount(), 1u);
    const ffpsd::Image gray = doc.GetLayerByIndex(0)->GetPixels();
    EXPECT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.bytes, (std::vector<std::uint8_t>{54, 182, 18, 255, 255, 128, 0, 64}));
    EXPECT_EQ(doc.GetMergedImage().bytes, (std::vector<std::uint8_t>{54, 182, 18, 255}));
    EXPECT_TRUE(doc.HasRealMergedData());
    EXPECT_EQ(doc.GetImageResourceById(1039), nullptr); // an RGB profile does not describe gray

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    EXPECT_EQ(back.GetColorMode(), ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, gray.bytes);

    // A composite's alpha channels stay as they are.
    ffpsd::Document channels = NewDocument();
    const ffpsd::Image five = Pattern(4, 3, 5);
    channels.SetMergedImage(five);
    channels.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(channels.GetChannelCount(), 3u);
    EXPECT_EQ(channels.GetMergedImage().bytes, ExpectedGray(five));
}

TEST(DocumentColorModeTest, APhotoshopFileTurnsGrayLayerByLayer)
{
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);

    doc.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    ASSERT_EQ(back.GetLayerCount(), 2u);
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    for (std::size_t i = 0; i < 2; ++i)
    {
        const ffpsd::Image rgb = original.GetLayerByIndex(i)->GetPixels();
        const ffpsd::Image gray = back.GetLayerByIndex(i)->GetPixels();
        EXPECT_EQ(gray.channel_count, rgb.channel_count - 2u) << "layer " << i;
        EXPECT_EQ(gray.bytes, ExpectedGray(rgb)) << "layer " << i;
        EXPECT_EQ(back.GetLayerByIndex(i)->GetBounds().right, original.GetLayerByIndex(i)->GetBounds().right);
    }

    // Only raster layers: the composite converts exactly, and stays real.
    EXPECT_TRUE(back.HasRealMergedData());
    EXPECT_EQ(back.GetMergedImage().bytes, ExpectedGray(original.GetMergedImage()));
}

TEST(DocumentColorModeTest, GrayToRgbCopiesThePlaneThreeTimes)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 8, 3, 1);
    ffpsd::Image layer;
    layer.width = 3;
    layer.height = 1;
    layer.channel_count = 2;
    layer.color_mode = ffpsd::ColorMode::kGrayscale;
    layer.bytes = {10, 128, 250, 255, 128, 0};
    doc.AddLayer("grays", layer);
    ffpsd::Image composite = layer;
    composite.channel_count = 1;
    composite.bytes.resize(3);
    doc.SetMergedImage(composite);

    doc.ConvertColorMode(ffpsd::ColorMode::kRgb);

    EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kRgb);
    EXPECT_EQ(doc.GetChannelCount(), 3u);
    EXPECT_EQ(
        doc.GetLayerByIndex(0)->GetPixels().bytes, (std::vector<std::uint8_t>{10, 128, 250, 10, 128, 250, 10, 128, 250, 255, 128, 0}));
    EXPECT_EQ(doc.GetMergedImage().bytes, (std::vector<std::uint8_t>{10, 128, 250, 10, 128, 250, 10, 128, 250}));
    EXPECT_TRUE(doc.HasRealMergedData());

    // A composite's alpha channels stay as they are.
    ffpsd::Document channels = NewDocument(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Image three = Pattern(4, 3, 3, 8, ffpsd::ColorMode::kGrayscale);
    channels.SetMergedImage(three);
    channels.ConvertColorMode(ffpsd::ColorMode::kRgb);
    EXPECT_EQ(channels.GetChannelCount(), 5u);
    EXPECT_EQ(channels.GetMergedImage().bytes, ExpectedRgb(three));
}

TEST(DocumentColorModeTest, APhotoshopGrayFileTurnsRgbAndBack)
{
    const ffpsd::Document original = ffpsd::Document::Open(kGrayscalePsd);
    ffpsd::Document doc = ffpsd::Document::Open(kGrayscalePsd);

    doc.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    ASSERT_EQ(back.GetLayerCount(), 2u);
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    for (std::size_t i = 0; i < 2; ++i)
        EXPECT_EQ(back.GetLayerByIndex(i)->GetPixels().bytes, ExpectedRgb(original.GetLayerByIndex(i)->GetPixels())) << "layer " << i;
    EXPECT_TRUE(back.HasRealMergedData());
    EXPECT_EQ(back.GetMergedImage().bytes, ExpectedRgb(original.GetMergedImage()));

    // Three equal channels have that same luma: the weights add up to exactly 1.
    doc.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    for (std::size_t i = 0; i < 2; ++i)
        EXPECT_EQ(doc.GetLayerByIndex(i)->GetPixels().bytes, original.GetLayerByIndex(i)->GetPixels().bytes) << "layer " << i;
    EXPECT_EQ(doc.GetMergedImage().bytes, original.GetMergedImage().bytes);
}

TEST(DocumentColorModeTest, RgbLevelsMoveToTheGrayRecordAndBack)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);

    doc.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    // Photoshop's 70 to 200 on every RGB channel lands in record 1, where a gray document keeps its channel.
    const std::optional<ffpsd::LevelsInfo> gray = back.GetLayerByIndex(1)->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_TRUE(gray.has_value());
    ASSERT_EQ(gray->channels.size(), 2u);
    ExpectRecord(gray->channels[0], 0, 255, "all");
    ExpectRecord(gray->channels[1], 70, 200, "gray");

    // Levels is not linear, so the gray of the old composite would not be what the layers give.
    EXPECT_FALSE(back.HasRealMergedData());
    const ffpsd::Image merged = back.GetMergedImage();
    EXPECT_EQ(merged.channel_count, 1u);
    EXPECT_EQ(PlaneSum(merged, 0), 0u);

    // Record 0 is Photoshop's 70 to 200 again; the channel records of the RGB file are gone with the gray step.
    doc.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::LevelsInfo rgb = *doc.GetLayerByIndex(1)->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_EQ(rgb.channels.size(), 4u);
    ExpectRecord(rgb.channels[0], 70, 200, "all");
    for (std::size_t i = 1; i < 4; ++i)
        ExpectRecord(rgb.channels[i], 0, 255, "channel");
}

TEST(DocumentColorModeTest, GrayLevelsBecomeTheRecordOfEveryRgbChannel)
{
    ffpsd::Document doc = ffpsd::Document::Open(kGrayscaleLevelsPsd);

    doc.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    // Photoshop's 25 to 237 on Gray is record 0 now; red, green and blue stay the identity.
    const ffpsd::LevelsInfo levels = *back.GetLayerByIndex(2)->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_EQ(levels.channels.size(), 4u);
    ExpectRecord(levels.channels[0], 25, 237, "all");
    for (std::size_t i = 1; i < 4; ++i)
        ExpectRecord(levels.channels[i], 0, 255, "channel");

    // The same Levels on three equal channels gives what it gave on gray.
    EXPECT_TRUE(back.HasRealMergedData());

    // With both records set, record 0 still applies to every channel, and every channel gets the gray one.
    ffpsd::Document both_doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    ffpsd::LevelsInfo both;
    both.channels.resize(2);
    both.channels[0].input_floor = 20;
    both.channels[1].input_floor = 30;
    both_doc.AddAdjustmentLayer("levels", both);
    both_doc.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::LevelsInfo rgb = *both_doc.GetLayerByIndex(0)->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_EQ(rgb.channels.size(), 4u);
    ExpectRecord(rgb.channels[0], 20, 255, "all");
    for (std::size_t i = 1; i < 4; ++i)
        ExpectRecord(rgb.channels[i], 30, 255, "channel");
}
