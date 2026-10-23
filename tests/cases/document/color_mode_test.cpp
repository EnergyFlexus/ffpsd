#include "support/test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
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
    ffpsd::Bytes ExpectedGray(const ffpsd::Image& rgb)
    {
        const std::size_t plane = std::size_t{rgb.width} * rgb.height;
        ffpsd::Bytes gray(plane * (rgb.channel_count - 2u));
        for (std::size_t i = 0; i < plane; ++i)
            gray[i] = Luma(rgb.bytes[i], rgb.bytes[plane + i], rgb.bytes[2 * plane + i]);
        std::copy(rgb.bytes.begin() + 3 * plane, rgb.bytes.end(), gray.begin() + plane);
        return gray;
    }

    // The RGB planes a gray image should become: the gray plane three times, then every plane after it.
    ffpsd::Bytes ExpectedRgb(const ffpsd::Image& gray)
    {
        const std::size_t plane = std::size_t{gray.width} * gray.height;
        ffpsd::Bytes rgb(plane * (gray.channel_count + 2u));
        for (std::size_t i = 0; i < 3; ++i)
            std::copy(gray.bytes.begin(), gray.bytes.begin() + plane, rgb.begin() + i * plane);
        std::copy(gray.bytes.begin() + plane, gray.bytes.end(), rgb.begin() + 3 * plane);
        return rgb;
    }

    // A document of one layer whose composite is the layer's colors, opaque.
    ffpsd::Document OneLayer(const ffpsd::Image& layer)
    {
        ffpsd::Document doc = NewDocument(layer.color_mode, 8, layer.width, layer.height);
        doc.AddLayer("colors", layer);
        ffpsd::Image composite = layer;
        --composite.channel_count;
        composite.bytes = ffpsd::Bytes(layer.bytes.data(), composite.GetSizeBytes());
        doc.SetMergedImage(composite);
        return doc;
    }

    // Record 0 for every channel at once, then count - 1 records of one channel each.
    void ExpectLevels(
        const ffpsd::Layer& layer, std::size_t count, std::uint16_t all_floor, std::uint16_t all_ceiling, std::uint16_t channel_floor,
        std::uint16_t channel_ceiling)
    {
        const std::optional<ffpsd::LevelsInfo> levels = layer.GetAdjustment<ffpsd::LevelsInfo>();
        ASSERT_TRUE(levels.has_value());
        ASSERT_EQ(levels->channels.size(), count);
        ExpectLevelsRecord(levels->channels[0], all_floor, all_ceiling, "all");
        for (std::size_t i = 1; i < count; ++i)
            ExpectLevelsRecord(levels->channels[i], channel_floor, channel_ceiling, "channel " + std::to_string(i));
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

TEST(DocumentColorModeTest, RgbBecomesItsLumaAndGrayThreeEqualChannels)
{
    // Red, green, blue and white at four alphas.
    ffpsd::Document rgb = OneLayer(Planes(4, 1, 4, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 128, 0, 64}));
    ffpsd::ImageResource profile;
    profile.id = 1039;
    profile.data = {1, 2, 3};
    rgb.SetImageResource(profile);

    rgb.ConvertColorMode(ffpsd::ColorMode::kGrayscale);

    // 6966 * 255 >> 15 is 54, 23436 * 255 >> 15 is 182, 2366 * 255 >> 15 is 18; white stays 255.
    EXPECT_EQ(rgb.GetColorMode(), ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(rgb.GetChannelCount(), 1u);
    const ffpsd::Image gray = rgb.GetLayerByIndex(0)->GetPixels();
    EXPECT_EQ(gray.channel_count, 2u);
    EXPECT_EQ(gray.bytes, (ffpsd::Bytes{54, 182, 18, 255, 255, 128, 0, 64}));
    EXPECT_EQ(rgb.GetMergedImage().bytes, (ffpsd::Bytes{54, 182, 18, 255}));
    EXPECT_TRUE(rgb.HasRealMergedData());
    EXPECT_EQ(rgb.GetImageResourceById(1039), nullptr); // an RGB profile does not describe gray
    const ffpsd::Document back = RoundTrip(rgb);
    EXPECT_EQ(back.GetColorMode(), ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(Pixels(back, 0), gray.bytes);

    ffpsd::Document grays = OneLayer(Planes(3, 1, 2, {10, 128, 250, 255, 128, 0}));

    grays.ConvertColorMode(ffpsd::ColorMode::kRgb);

    EXPECT_EQ(grays.GetColorMode(), ffpsd::ColorMode::kRgb);
    EXPECT_EQ(grays.GetChannelCount(), 3u);
    EXPECT_EQ(Pixels(grays, 0), (ffpsd::Bytes{10, 128, 250, 10, 128, 250, 10, 128, 250, 255, 128, 0}));
    EXPECT_EQ(grays.GetMergedImage().bytes, (ffpsd::Bytes{10, 128, 250, 10, 128, 250, 10, 128, 250}));
    EXPECT_TRUE(grays.HasRealMergedData());

    // A composite's alpha channels stay as they are, both ways.
    ffpsd::Document five = NewDocument();
    const ffpsd::Image five_planes = Pattern(4, 3, 5);
    five.SetMergedImage(five_planes);
    five.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(five.GetChannelCount(), 3u);
    EXPECT_EQ(five.GetMergedImage().bytes, ExpectedGray(five_planes));

    ffpsd::Document three = NewDocument(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Image three_planes = Pattern(4, 3, 3, 8, ffpsd::ColorMode::kGrayscale);
    three.SetMergedImage(three_planes);
    three.ConvertColorMode(ffpsd::ColorMode::kRgb);
    EXPECT_EQ(three.GetChannelCount(), 5u);
    EXPECT_EQ(three.GetMergedImage().bytes, ExpectedRgb(three_planes));
}

TEST(DocumentColorModeTest, PhotoshopFilesConvertLayerByLayer)
{
    const ffpsd::Document rgb_file = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Document rgb = ffpsd::Document::Open(kRgbPsd);
    rgb.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Document gray_back = RoundTrip(rgb);

    ASSERT_EQ(gray_back.GetLayerCount(), 2u);
    EXPECT_TRUE(gray_back.GetLayerByIndex(0)->IsBackground());
    for (std::size_t i = 0; i < 2; ++i)
    {
        const ffpsd::Image before = rgb_file.GetLayerByIndex(i)->GetPixels();
        const ffpsd::Image after = gray_back.GetLayerByIndex(i)->GetPixels();
        EXPECT_EQ(after.channel_count, before.channel_count - 2u) << "layer " << i;
        EXPECT_EQ(after.bytes, ExpectedGray(before)) << "layer " << i;
        EXPECT_EQ(gray_back.GetLayerByIndex(i)->GetBounds().right, rgb_file.GetLayerByIndex(i)->GetBounds().right);
    }

    // Only raster layers: the composite converts exactly, and stays real.
    EXPECT_TRUE(gray_back.HasRealMergedData());
    EXPECT_EQ(gray_back.GetMergedImage().bytes, ExpectedGray(rgb_file.GetMergedImage()));

    const ffpsd::Document gray_file = ffpsd::Document::Open(kGrayscalePsd);
    ffpsd::Document gray = ffpsd::Document::Open(kGrayscalePsd);
    gray.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::Document rgb_back = RoundTrip(gray);

    ASSERT_EQ(rgb_back.GetLayerCount(), 2u);
    EXPECT_TRUE(rgb_back.GetLayerByIndex(0)->IsBackground());
    for (std::size_t i = 0; i < 2; ++i)
        EXPECT_EQ(Pixels(rgb_back, i), ExpectedRgb(gray_file.GetLayerByIndex(i)->GetPixels())) << "layer " << i;
    EXPECT_TRUE(rgb_back.HasRealMergedData());
    EXPECT_EQ(rgb_back.GetMergedImage().bytes, ExpectedRgb(gray_file.GetMergedImage()));

    // Three equal channels have that same luma: the weights add up to exactly 1.
    gray.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    for (std::size_t i = 0; i < 2; ++i)
        EXPECT_EQ(Pixels(gray, i), Pixels(gray_file, i)) << "layer " << i;
    EXPECT_EQ(gray.GetMergedImage().bytes, gray_file.GetMergedImage().bytes);
}

TEST(DocumentColorModeTest, LevelsMoveBetweenTheGrayAndTheRgbRecords)
{
    // Photoshop's 70 to 200 on every RGB channel lands in record 1, where a gray document keeps its channel.
    ffpsd::Document rgb = ffpsd::Document::Open(kRgbLevelsPsd);
    rgb.ConvertColorMode(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Document gray_back = RoundTrip(rgb);
    ExpectLevels(*gray_back.GetLayerByIndex(1), 2, 0, 255, 70, 200);

    // Levels is not linear, so the gray of the old composite would not be what the layers give.
    EXPECT_FALSE(gray_back.HasRealMergedData());
    const ffpsd::Image merged = gray_back.GetMergedImage();
    EXPECT_EQ(merged.channel_count, 1u);
    EXPECT_EQ(PlaneSum(merged, 0), 0u);

    // Record 0 is Photoshop's 70 to 200 again; the channel records of the RGB file are gone with the gray step.
    rgb.ConvertColorMode(ffpsd::ColorMode::kRgb);
    ExpectLevels(*rgb.GetLayerByIndex(1), 4, 70, 200, 0, 255);

    // Photoshop's 25 to 237 on Gray is record 0 now; red, green and blue stay the identity.
    ffpsd::Document gray = ffpsd::Document::Open(kGrayscaleLevelsPsd);
    gray.ConvertColorMode(ffpsd::ColorMode::kRgb);
    const ffpsd::Document rgb_back = RoundTrip(gray);
    ExpectLevels(*rgb_back.GetLayerByIndex(2), 4, 25, 237, 0, 255);

    // The same Levels on three equal channels gives what it gave on gray.
    EXPECT_TRUE(rgb_back.HasRealMergedData());

    // With both records set, record 0 still applies to every channel, and every channel gets the gray one.
    ffpsd::Document both = NewDocument(ffpsd::ColorMode::kGrayscale);
    ffpsd::LevelsInfo records;
    records.channels.resize(2);
    records.channels[0].input_floor = 20;
    records.channels[1].input_floor = 30;
    both.AddAdjustmentLayer("levels", records);
    both.ConvertColorMode(ffpsd::ColorMode::kRgb);
    ExpectLevels(*both.GetLayerByIndex(0), 4, 20, 255, 30, 255);
}
