#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

using namespace ffpsd_test;

TEST(LayerResizeTest, SetPositionMovesThePixelsAsTheyAre)
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
    EXPECT_FALSE(doc.GetHasRealMergedData());
}

TEST(LayerResizeTest, SetPositionRefusesBoundsPastThirtyTwoBits)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("moved", Pattern(3, 2, 4));

    EXPECT_THROW(layer->SetPosition(0, std::numeric_limits<std::int32_t>::max() - 1), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().left, 0);
}

TEST(LayerResizeTest, ResizeKeepsTheTopLeftCorner)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Image before = Pattern(3, 2, 4);
    ffpsd::Layer* layer = doc.AddLayer("resized", before, 2, 5);

    layer->Resize(7, 5);

    const ffpsd::Rect bounds = layer->GetBounds();
    EXPECT_EQ(bounds.top, 2);
    EXPECT_EQ(bounds.left, 5);
    EXPECT_EQ(bounds.GetWidth(), 7);
    EXPECT_EQ(bounds.GetHeight(), 5);
    EXPECT_EQ(layer->GetPixels().channel_count, 4u);
}

TEST(LayerResizeTest, NearestDoublesAPhotoshopLayer)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kGrayscalePsd);
    ffpsd::Layer* fill = doc.GetLayerByIndex(1);
    const ffpsd::Image before = fill->GetPixels();

    fill->Resize(before.width * 2, before.height * 2, ffpsd::ResampleFilter::kNearest);

    // Every source sample appears four times, so each plane's sum is four times as large.
    const ffpsd::Image after = fill->GetPixels();
    EXPECT_EQ(PlaneSum(after, 0), 4 * PlaneSum(before, 0));
    EXPECT_EQ(PlaneSum(after, 1), 4 * PlaneSum(before, 1));
}

TEST(LayerResizeTest, AResizedLayerSurvivesSaving)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 16);
    ffpsd::Layer* layer = doc.AddLayer("deep", Pattern(4, 3, 4, 16));
    layer->Resize(9, 2);
    const ffpsd::Image expected = layer->GetPixels();

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerByIndex(0)->GetBounds().GetWidth(), 9);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, expected.bytes);
}

TEST(LayerResizeTest, WhatCannotMoveOrResizeIsRefused)
{
    ffpsd::Document rgb = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::Layer* background = rgb.GetLayerByIndex(0);
    EXPECT_THROW(background->SetPosition(1, 0), std::invalid_argument);
    EXPECT_THROW(background->Resize(10, 10), std::invalid_argument);
    background->SetPosition(0, 0); // where it is already

    ffpsd::Document levels = ffpsd::Document::Parse(kRgbLevelsPsd);
    EXPECT_THROW(levels.GetLayerByIndex(1)->Resize(10, 10), std::invalid_argument);
    EXPECT_THROW(levels.GetLayerByIndex(1)->SetPosition(3, 3), std::invalid_argument);

    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* empty = doc.AddLayer("empty");
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(3, 2, 4));
    EXPECT_THROW(empty->Resize(4, 4), std::invalid_argument);
    EXPECT_THROW(layer->Resize(0, 4), std::invalid_argument);
    EXPECT_THROW(layer->Resize(30001, 4), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().GetWidth(), 3);

    SetSectionDivider(*layer, 3);
    EXPECT_THROW(layer->Resize(4, 4), std::invalid_argument);
}
