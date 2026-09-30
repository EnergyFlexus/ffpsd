// Photoshop 2026 files; the expected values come from an independent dump of them.
#include "support/test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

using namespace ffpsd_test;

TEST(SmokeTest, AGrayscaleFileReadsAsTheDumpSays)
{
    const ffpsd::Document doc = ffpsd::Document::Open(kGrayscalePsd);

    EXPECT_EQ(doc.GetWidth(), 836u);
    EXPECT_EQ(doc.GetHeight(), 1200u);
    EXPECT_EQ(doc.GetChannelCount(), 1u);
    EXPECT_EQ(doc.GetDepth(), 8u);
    EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kGrayscale);
    EXPECT_FALSE(doc.IsPsb());

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

    // A background has no transparency; the layer above it has, as the last plane.
    const ffpsd::Image background_pixels = background->GetPixels();
    const ffpsd::Image fill_pixels = fill->GetPixels();
    ASSERT_EQ(background_pixels.channel_count, 1u);
    ASSERT_EQ(fill_pixels.channel_count, 2u);
    EXPECT_EQ(background_pixels.width, 836u);
    EXPECT_EQ(background_pixels.height, 1200u);
    EXPECT_EQ(PlaneSum(background_pixels, 0), 201492513u);
    EXPECT_EQ(PlaneSum(fill_pixels, 0), 207820579u);
    EXPECT_EQ(PlaneSum(fill_pixels, 1), 255816000u);
}

TEST(SmokeTest, AnRgbFileReadsAsTheDumpSays)
{
    const ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);

    EXPECT_EQ(doc.GetImageResourceCount(), 27u);
    // Set up in pixels per centimeter, so Photoshop stores 0x012BFFFE, just under 300 ppi.
    const ffpsd::ResolutionInfo resolution = doc.GetResolutionInfo();
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

    ASSERT_EQ(doc.GetLayerCount(), 2u);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetName(), kBackgroundName);
    EXPECT_EQ(doc.GetLayerByIndex(1)->GetName(), kBackgroundCopyName);
    const ffpsd::Image background = doc.GetLayerByIndex(0)->GetPixels();
    const ffpsd::Image copy = doc.GetLayerByIndex(1)->GetPixels();
    ASSERT_EQ(background.channel_count, 3u);
    ASSERT_EQ(copy.channel_count, 4u);
    EXPECT_EQ(PlaneSum(background, 0), 682923150u);
    EXPECT_EQ(PlaneSum(copy, 0), 678522862u);
    EXPECT_EQ(PlaneSum(copy, 1), 641365113u);
    EXPECT_EQ(PlaneSum(copy, 2), 641365113u);
    EXPECT_EQ(PlaneSum(copy, 3), 682923150u);

    const ffpsd::Image merged = doc.GetMergedImage();
    ASSERT_EQ(merged.width, 1890u);
    ASSERT_EQ(merged.height, 1417u);
    ASSERT_EQ(merged.channel_count, 3u);
    ASSERT_EQ(merged.depth, 8u);
    EXPECT_EQ(PlaneSum(merged, 0), 678522862u);
    EXPECT_EQ(PlaneSum(merged, 1), 641365113u);
    EXPECT_EQ(PlaneSum(merged, 2), 641365113u);

    // The blocks after the layers, in file order; three of them had padding to skip.
    const std::vector<std::pair<std::uint32_t, std::size_t>> blocks = {{Fourcc("Patt"), 0},  {Fourcc("CAI "), 77}, {Fourcc("OCIO"), 172},
                                                                       {Fourcc("GenI"), 84}, {Fourcc("FMsk"), 12}, {Fourcc("cinf"), 410}};
    ASSERT_EQ(doc.GetTaggedBlockCount(), blocks.size());
    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        EXPECT_EQ(doc.GetTaggedBlockByIndex(i)->key, blocks[i].first) << "block " << i;
        EXPECT_EQ(doc.GetTaggedBlockByIndex(i)->data.size(), blocks[i].second) << "block " << i;
    }
}
