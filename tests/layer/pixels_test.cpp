#include "support/test_support.hpp"

#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(LayerPixelsTest, AddLayerKeepsEveryPlaneInOrder)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Image opaque = doc.AddLayer("rgb", Pattern(3, 2, 3))->GetPixels();
    const ffpsd::Image transparent = doc.AddLayer("rgba", Pattern(3, 2, 4))->GetPixels();

    EXPECT_EQ(opaque.channel_count, 3u);
    EXPECT_EQ(opaque.bytes, Pattern(3, 2, 3).bytes);
    EXPECT_EQ(transparent.channel_count, 4u);
    EXPECT_EQ(transparent.bytes, Pattern(3, 2, 4).bytes);
}

TEST(LayerPixelsTest, TheLayerSitsWhereItWasPut)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Rect bounds = doc.AddLayer("off canvas", Pattern(3, 2, 3), -5, 7)->GetBounds();

    EXPECT_EQ(bounds.top, -5);
    EXPECT_EQ(bounds.left, 7);
    EXPECT_EQ(bounds.bottom, -3);
    EXPECT_EQ(bounds.right, 10);
}

TEST(LayerPixelsTest, AnEmptyLayerHasNoPixels)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Layer* layer = doc.AddLayer("empty", ffpsd::Image(), 2, 1);

    EXPECT_EQ(layer->GetBounds().GetWidth(), 0);
    EXPECT_EQ(layer->GetBounds().top, 2);
    EXPECT_TRUE(layer->GetPixels().bytes.empty());
}

TEST(LayerPixelsTest, ThePlanarOverloadTakesTheSameBytes)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Image image = Pattern(3, 2, 4);

    const ffpsd::Layer* layer = doc.AddLayer("planar", image.bytes.data(), image.bytes.size(), 3, 2, 4);

    EXPECT_EQ(layer->GetPixels().bytes, image.bytes);
}

TEST(LayerPixelsTest, SixteenBitSamplesStayInNativeOrder)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 16);
    ffpsd::Image image = Pattern(2, 1, 3, 16);
    const std::uint16_t first = 0x1234;
    std::memcpy(image.bytes.data(), &first, sizeof(first));

    const ffpsd::Image back = doc.AddLayer("deep", image)->GetPixels();

    std::uint16_t read = 0;
    std::memcpy(&read, back.bytes.data(), sizeof(read));
    EXPECT_EQ(read, 0x1234);
    EXPECT_EQ(back.bytes, image.bytes);
}

TEST(LayerPixelsTest, AddLayerRefusesImagesThatDoNotFit)
{
    ffpsd::Document doc = NewDocument();

    EXPECT_THROW(doc.AddLayer("gray", Pattern(2, 2, 1)), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("five", Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("deep", Pattern(2, 2, 3, 16)), std::invalid_argument);

    const ffpsd::Image image = Pattern(2, 2, 3);
    EXPECT_THROW(doc.AddLayer("short", image.bytes.data(), image.bytes.size() - 1, 2, 2, 3), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("wide", nullptr, 0, 30001, 1, 3), std::invalid_argument);

    EXPECT_EQ(doc.GetLayerCount(), 0u);
}

TEST(LayerPixelsTest, SomeColorModesHaveNoLayers)
{
    ffpsd::Document doc = NewDocument();
    doc.SetColorMode(ffpsd::ColorMode::kIndexed);

    EXPECT_THROW(doc.AddLayer("indexed"), std::invalid_argument);
    EXPECT_THROW(doc.AddAdjustmentLayer<ffpsd::LevelsInfo>("indexed"), std::invalid_argument);
}

TEST(LayerPixelsTest, SetWhatGetGaveKeepsEverySample)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::Layer* layer = doc.GetLayerByIndex(1);
    const ffpsd::Image before = layer->GetPixels();
    const ffpsd::Rect bounds = layer->GetBounds();

    layer->SetPixels(before);

    EXPECT_EQ(layer->GetPixels().bytes, before.bytes);
    EXPECT_EQ(layer->GetBounds().right, bounds.right);
    EXPECT_EQ(layer->GetBounds().bottom, bounds.bottom);
    EXPECT_FALSE(doc.HasRealMergedData());
}

TEST(LayerPixelsTest, ANewSizeKeepsTheTopLeftCorner)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("moved", Pattern(4, 3, 3), 5, 6);

    layer->SetPixels(Pattern(2, 1, 4));

    EXPECT_EQ(layer->GetBounds().top, 5);
    EXPECT_EQ(layer->GetBounds().left, 6);
    EXPECT_EQ(layer->GetBounds().GetWidth(), 2);
    EXPECT_EQ(layer->GetBounds().GetHeight(), 1);
    EXPECT_EQ(layer->GetPixels().bytes, Pattern(2, 1, 4).bytes);
}

TEST(LayerPixelsTest, SetPositionMovesThePixelsAsTheyAre)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("moved", Pattern(3, 2, 4), 1, 1);
    doc.SetHasRealMergedData(true);

    layer->SetPosition(-4, 7);

    const ffpsd::Rect bounds = layer->GetBounds();
    EXPECT_EQ(bounds.top, -4);
    EXPECT_EQ(bounds.left, 7);
    EXPECT_EQ(bounds.bottom, -2);
    EXPECT_EQ(bounds.right, 10);
    EXPECT_EQ(layer->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());
}

TEST(LayerPixelsTest, SetPositionRefusesBoundsPastThirtyTwoBits)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("moved", Pattern(3, 2, 4));

    EXPECT_THROW(layer->SetPosition(0, std::numeric_limits<std::int32_t>::max() - 1), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().left, 0);
}

TEST(LayerPixelsTest, AWrongImageIsRefusedAndTheLayerStays)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::Layer* layer = doc.GetLayerByIndex(1);
    const ffpsd::Image before = layer->GetPixels();

    EXPECT_THROW(layer->SetPixels(Pattern(3, 2, 1)), std::invalid_argument);
    EXPECT_THROW(layer->SetPixels(Pattern(3, 2, 4, 16)), std::invalid_argument);

    EXPECT_EQ(layer->GetBounds().GetWidth(), 1890);
    EXPECT_EQ(layer->GetPixels().bytes, before.bytes);
}

TEST(LayerPixelsTest, NewPixelsLeaveTheMaskAlone)
{
    // Photoshop's Levels layer has a layer mask channel next to its empty ones.
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbLevelsPsd);
    ffpsd::Layer* levels = doc.GetLayerByIndex(1);
    doc.SetHasRealMergedData(false);
    const std::vector<std::uint8_t> before = doc.Save();

    levels->SetPixels(ffpsd::Image());

    EXPECT_EQ(doc.Save(), before);
}
