// Photoshop 2026 files; the expected values come from an independent dump of them.
#include "support/test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <utility>
#include <vector>

using namespace ffpsd_test;

TEST(ReadPsdTest, HeaderOfAGrayscaleFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kGrayscalePsd);

    EXPECT_EQ(doc.GetWidth(), 836u);
    EXPECT_EQ(doc.GetHeight(), 1200u);
    EXPECT_EQ(doc.GetChannelCount(), 1u);
    EXPECT_EQ(doc.GetDepth(), 8u);
    EXPECT_EQ(doc.GetColor(), ffpsd::ColorMode::kGrayscale);
    EXPECT_FALSE(doc.IsPsb());
}

TEST(ReadPsdTest, LayersOfAGrayscaleFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kGrayscalePsd);
    ASSERT_EQ(doc.GetLayerCount(), 2u);

    const ffpsd::Layer* background = doc.GetLayerByIndex(0);
    const ffpsd::Layer* fill = doc.GetLayerByIndex(1);
    EXPECT_EQ(background->GetName(), kBackgroundName);
    EXPECT_EQ(fill->GetName(), kColorFillName);

    for (const ffpsd::Layer* layer : {background, fill})
    {
        EXPECT_EQ(layer->GetKind(), ffpsd::LayerKind::kRaster);
        EXPECT_TRUE(layer->IsVisible());
        EXPECT_EQ(layer->GetOpacity(), 255u);
        EXPECT_EQ(layer->GetBlendKey(), Fourcc("norm"));
        EXPECT_EQ(layer->GetBounds().GetWidth(), 836);
        EXPECT_EQ(layer->GetBounds().GetHeight(), 1200);
    }

    // Ids need not be consecutive: layers 2 to 4 were created and deleted in Photoshop.
    EXPECT_EQ(LayerId(*background), 1u);
    EXPECT_EQ(LayerId(*fill), 5u);
}

TEST(ReadPsdTest, LayerPixelsOfAGrayscaleFile)
{
    // A background has no transparency; the layer above it has, as the last plane.
    const ffpsd::Document doc = ffpsd::Document::Parse(kGrayscalePsd);
    const ffpsd::Image background = doc.GetLayerByIndex(0)->GetPixels();
    const ffpsd::Image fill = doc.GetLayerByIndex(1)->GetPixels();

    ASSERT_EQ(background.channel_count, 1u);
    ASSERT_EQ(fill.channel_count, 2u);
    EXPECT_EQ(background.width, 836u);
    EXPECT_EQ(background.height, 1200u);
    EXPECT_EQ(PlaneSum(background, 0), 201492513u);
    EXPECT_EQ(PlaneSum(fill, 0), 207820579u);
    EXPECT_EQ(PlaneSum(fill, 1), 255816000u);
}

TEST(ReadPsdTest, ImageResourcesOfAnRgbFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    EXPECT_EQ(doc.GetImageResourceCount(), 27u);

    const ffpsd::ResolutionInfo resolution = doc.GetResolutionInfo();
    // Set up in pixels per centimeter, so Photoshop stores 0x012BFFFE, just under 300 ppi.
    EXPECT_DOUBLE_EQ(resolution.horizontal, 0x012BFFFE / 65536.0);
    EXPECT_DOUBLE_EQ(resolution.vertical, 0x012BFFFE / 65536.0);
    EXPECT_EQ(resolution.horizontal_unit, 2);
    EXPECT_EQ(resolution.width_unit, 2);

    const ffpsd::VersionInfo version = doc.GetVersionInfo();
    EXPECT_EQ(version.version, 1u);
    EXPECT_TRUE(version.has_real_merged_data);
    EXPECT_EQ(version.writer_name, "Adobe Photoshop");
    EXPECT_EQ(version.reader_name, "Adobe Photoshop 2026");

    // 1044, the last layer id handed out, and 1024, the target layer counted from the bottom.
    const ffpsd::ImageResource* seed = doc.GetImageResourceById(1044);
    const ffpsd::ImageResource* target = doc.GetImageResourceById(1024);
    ASSERT_NE(seed, nullptr);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(BigEndianU32(seed->data), 2u);
    EXPECT_EQ(target->data, (std::vector<std::uint8_t>{0, 1}));
}

TEST(ReadPsdTest, LayersAndMergedImageOfAnRgbFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ASSERT_EQ(doc.GetLayerCount(), 2u);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetName(), kBackgroundName);
    EXPECT_EQ(doc.GetLayerByIndex(1)->GetName(), kBackgroundCopyName);

    // RLE in the file.
    const ffpsd::Image merged = doc.GetMergedImage();
    ASSERT_EQ(merged.width, 1890u);
    ASSERT_EQ(merged.height, 1417u);
    ASSERT_EQ(merged.channel_count, 3u);
    ASSERT_EQ(merged.depth, 8u);
    EXPECT_EQ(PlaneSum(merged, 0), 678522862u);
    EXPECT_EQ(PlaneSum(merged, 1), 641365113u);
    EXPECT_EQ(PlaneSum(merged, 2), 641365113u);
}

TEST(ReadPsdTest, LayerPixelsOfAnRgbFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Image background = doc.GetLayerByIndex(0)->GetPixels();
    const ffpsd::Image copy = doc.GetLayerByIndex(1)->GetPixels();

    ASSERT_EQ(background.channel_count, 3u);
    ASSERT_EQ(copy.channel_count, 4u);
    EXPECT_EQ(PlaneSum(background, 0), 682923150u);
    EXPECT_EQ(PlaneSum(copy, 0), 678522862u);
    EXPECT_EQ(PlaneSum(copy, 1), 641365113u);
    EXPECT_EQ(PlaneSum(copy, 2), 641365113u);
    EXPECT_EQ(PlaneSum(copy, 3), 682923150u);
}

TEST(ReadPsdTest, SectionBlocksOfAnRgbFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    // The blocks after the layers, in file order; three of them had padding to skip.
    const std::vector<std::pair<std::uint32_t, std::size_t>> expected = {{Fourcc("Patt"), 0},  {Fourcc("CAI "), 77}, {Fourcc("OCIO"), 172},
                                                                         {Fourcc("GenI"), 84}, {Fourcc("FMsk"), 12}, {Fourcc("cinf"), 410}};
    ASSERT_EQ(doc.GetTaggedBlockCount(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(doc.GetTaggedBlockByIndex(i)->key, expected[i].first) << "block " << i;
        EXPECT_EQ(doc.GetTaggedBlockByIndex(i)->data.size(), expected[i].second) << "block " << i;
    }
}

TEST(ReadPsdTest, LevelsLayerOfAnRgbFile)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbLevelsPsd);
    ASSERT_EQ(doc.GetLayerCount(), 2u);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetKind(), ffpsd::LayerKind::kRaster);

    // An adjustment layer has no pixels of its own.
    const ffpsd::Layer* levels = doc.GetLayerByIndex(1);
    EXPECT_EQ(levels->GetName(), kLevelsName);
    EXPECT_EQ(levels->GetKind(), ffpsd::LayerKind::kAdjustment);
    EXPECT_EQ(levels->GetAdjustmentKey(), Fourcc("levl"));
    EXPECT_EQ(levels->GetBounds().GetWidth(), 0);
    EXPECT_TRUE(levels->GetPixels().bytes.empty());

    // All of RGB, then red, green and blue; every gamma left at 1.
    const std::optional<ffpsd::LevelsInfo> info = levels->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_TRUE(info.has_value());
    ASSERT_EQ(info->channels.size(), 4u);
    const std::uint16_t expected[4][4] = {{70, 200, 0, 255}, {10, 245, 0, 255}, {20, 250, 0, 255}, {10, 250, 0, 255}};
    for (std::size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(info->channels[i].input_floor, expected[i][0]) << "record " << i;
        EXPECT_EQ(info->channels[i].input_ceiling, expected[i][1]) << "record " << i;
        EXPECT_EQ(info->channels[i].output_floor, expected[i][2]) << "record " << i;
        EXPECT_EQ(info->channels[i].output_ceiling, expected[i][3]) << "record " << i;
        EXPECT_DOUBLE_EQ(info->channels[i].gamma, 1.0) << "record " << i;
    }

    // Photoshop's composite, with the Levels applied to the background.
    const ffpsd::Image merged = doc.GetMergedImage();
    EXPECT_EQ(PlaneSum(merged, 0), 612985261u);
    EXPECT_EQ(PlaneSum(merged, 1), 682923150u);
    EXPECT_EQ(PlaneSum(merged, 2), 640542472u);
}
