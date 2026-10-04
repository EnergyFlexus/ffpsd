#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>

using namespace ffpsd_test;

namespace
{
    void ExpectRect(const ffpsd::Rect& rect, std::int32_t top, std::int32_t left, std::int32_t bottom, std::int32_t right)
    {
        EXPECT_EQ(rect.top, top);
        EXPECT_EQ(rect.left, left);
        EXPECT_EQ(rect.bottom, bottom);
        EXPECT_EQ(rect.right, right);
    }
} // namespace

TEST(LayerMaskTest, PhotoshopsPixelMasksAreRead)
{
    const ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_FALSE(doc.GetLayerByIndex(1)->GetMask().has_value());

    const std::optional<ffpsd::LayerMask> plain = doc.GetLayerByIndex(2)->GetMask();
    ASSERT_TRUE(plain.has_value());
    ExpectRect(plain->bounds, 0, 935, 1417, 1890);
    EXPECT_EQ(plain->default_color, 255);
    EXPECT_EQ(plain->image.width, 955u);
    EXPECT_EQ(plain->image.height, 1417u);
    EXPECT_EQ(plain->image.channel_count, 1u);
    EXPECT_EQ(plain->image.color_mode, ffpsd::ColorMode::kGrayscale);

    // Beside a vector mask, -2 is Photoshop's rendering of it and the pixel mask is -3.
    const std::optional<ffpsd::LayerMask> real = doc.GetLayerByIndex(3)->GetMask();
    ASSERT_TRUE(real.has_value());
    ExpectRect(real->bounds, 0, 0, 1417, 703);
    EXPECT_EQ(real->image.width, 703u);
}

TEST(LayerMaskTest, ASetMaskIsSavedReplacedAndRemoved)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(4, 3, 4));
    const ffpsd::Image mask = Pattern(2, 2, 1);
    layer->SetMask(mask, 1, 2, 0);
    EXPECT_FALSE(doc.HasRealMergedData());

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    const std::optional<ffpsd::LayerMask> read = back.GetLayerByIndex(0)->GetMask();
    ASSERT_TRUE(read.has_value());
    ExpectRect(read->bounds, 1, 2, 3, 4);
    EXPECT_EQ(read->default_color, 0);
    EXPECT_EQ(read->image.bytes, mask.bytes);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(4, 3, 4).bytes);

    // An empty image is a mask of its default color alone.
    layer->SetMask(ffpsd::ImageView(), 0, 0, 255);
    const std::optional<ffpsd::LayerMask> blank = ffpsd::Document::Parse(doc.Save()).GetLayerByIndex(0)->GetMask();
    ASSERT_TRUE(blank.has_value());
    EXPECT_TRUE(blank->image.IsEmpty());
    EXPECT_EQ(blank->default_color, 255);

    EXPECT_TRUE(layer->RemoveMask());
    EXPECT_FALSE(layer->RemoveMask());
    EXPECT_FALSE(ffpsd::Document::Parse(doc.Save()).GetLayerByIndex(0)->GetMask().has_value());

    ffpsd::Document deep = NewDocument(ffpsd::ColorMode::kRgb, 16);
    ffpsd::Layer* deep_layer = deep.AddLayer("layer", Pattern(4, 3, 3, 16));
    deep_layer->SetMask(Pattern(3, 3, 1, 16), 0, 0);
    EXPECT_EQ(ffpsd::Document::Parse(deep.Save()).GetLayerByIndex(0)->GetMask()->image.bytes, Pattern(3, 3, 1, 16).bytes);
}

TEST(LayerMaskTest, AMaskMovesAndScalesWithItsLayer)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 8, 8);
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(2, 2, 4), 1, 1);
    layer->SetMask(Pattern(2, 2, 1), 2, 2, 0);

    layer->SetPosition(3, 2);
    ExpectRect(layer->GetMask()->bounds, 4, 3, 6, 5);

    layer->Resize(4, 4, ffpsd::ResampleFilter::kNearest);
    ExpectRect(layer->GetBounds(), 3, 2, 7, 6);
    const ffpsd::LayerMask mask = *layer->GetMask();
    ExpectRect(mask.bounds, 5, 4, 9, 8);
    const std::uint8_t a = Pattern(2, 2, 1).bytes[0];
    const std::uint8_t b = Pattern(2, 2, 1).bytes[1];
    const std::uint8_t c = Pattern(2, 2, 1).bytes[2];
    const std::uint8_t d = Pattern(2, 2, 1).bytes[3];
    EXPECT_EQ(mask.image.bytes, (std::vector<std::uint8_t>{a, a, b, b, a, a, b, b, c, c, d, d, c, c, d, d}));

    // Photoshop's masked layer moves now, its mask along.
    ffpsd::Document photoshop = ffpsd::Document::Open(kRgbMasksPsd);
    photoshop.GetLayerByIndex(2)->SetPosition(10, 20);
    ExpectRect(ffpsd::Document::Parse(photoshop.Save()).GetLayerByIndex(2)->GetMask()->bounds, 10, 955, 1427, 1910);
}

TEST(LayerMaskTest, WhatAMaskCannotGoOnIsRefused)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_THROW(doc.GetLayerByIndex(0)->SetMask(Pattern(2, 2, 1), 0, 0), std::invalid_argument);

    ffpsd::Layer* vector = doc.GetLayerByIndex(3);
    EXPECT_THROW(vector->SetMask(Pattern(2, 2, 1), 0, 0), std::invalid_argument);
    EXPECT_THROW(vector->RemoveMask(), std::invalid_argument);
    EXPECT_THROW(vector->SetPosition(1, 1), std::invalid_argument);
    EXPECT_THROW(vector->Resize(10, 10), std::invalid_argument);

    ffpsd::Layer* layer = doc.GetLayerByIndex(1);
    EXPECT_THROW(layer->SetMask(Pattern(2, 2, 3), 0, 0), std::invalid_argument);
    EXPECT_THROW(layer->SetMask(Pattern(2, 2, 1, 16), 0, 0), std::invalid_argument);
    EXPECT_THROW(layer->SetMask(Pattern(2, 2, 1), 0, 0, 7), std::invalid_argument);
    EXPECT_FALSE(layer->GetMask().has_value());
}
