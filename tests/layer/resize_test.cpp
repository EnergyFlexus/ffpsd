#include "support/test_support.hpp"

#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace ffpsd_test;

namespace
{
    // The pixels of a layer made from image and resized; gray for 1 or 2 planes, RGB for 3 or 4.
    ffpsd::Image Resized(
        const ffpsd::Image& image, std::uint32_t width, std::uint32_t height,
        ffpsd::ResampleFilter filter = ffpsd::ResampleFilter::kBicubic)
    {
        const ffpsd::ColorMode color = image.channel_count < 3 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        ffpsd::Document doc = NewDocument(color, image.depth);
        ffpsd::Layer* layer = doc.AddLayer("layer", image);
        layer->Resize(width, height, filter);
        return layer->GetPixels();
    }

    // One gray row per line, each row 0, 4, 8, ... left to right.
    ffpsd::Image Ramp(std::uint32_t width, std::uint32_t height)
    {
        ffpsd::Image image = Pattern(width, height, 1);
        for (std::size_t y = 0; y < height; ++y)
            for (std::size_t x = 0; x < width; ++x)
                image.bytes[y * width + x] = static_cast<std::uint8_t>(4 * x);
        return image;
    }
} // namespace

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

// The pixels Resize computes; expected values are worked out by hand from the filter definitions.

class LayerResizeFilterTest : public testing::TestWithParam<std::tuple<ffpsd::ResampleFilter, std::uint16_t>>
{
};

TEST_P(LayerResizeFilterTest, TheSameSizeGivesTheSameBytes)
{
    const auto [filter, depth] = GetParam();
    const ffpsd::Image image = Pattern(5, 3, 4, depth);

    EXPECT_EQ(Resized(image, 5, 3, filter).bytes, image.bytes);
}

TEST_P(LayerResizeFilterTest, AFlatImageStaysFlatAtAnyScale)
{
    const auto [filter, depth] = GetParam();
    ffpsd::Image flat = Pattern(6, 4, 1, depth);
    const std::size_t sample = flat.GetBytesPerSample();
    for (std::size_t i = 0; i < flat.bytes.size(); i += sample)
    {
        if (depth == 8)
        {
            flat.bytes[i] = 77;
        }
        else if (depth == 16)
        {
            const std::uint16_t value = 0x4D4D;
            std::memcpy(flat.bytes.data() + i, &value, 2);
        }
        else
        {
            const float value = 0.3f;
            std::memcpy(flat.bytes.data() + i, &value, 4);
        }
    }

    for (const auto& [width, height] : {std::make_pair(13u, 9u), std::make_pair(2u, 1u), std::make_pair(6u, 11u)})
    {
        const ffpsd::Image resized = Resized(flat, width, height, filter);
        ASSERT_EQ(resized.bytes.size(), std::size_t{width} * height * sample);
        for (std::size_t i = 0; i < resized.bytes.size(); i += sample)
        {
            // Float weights sum to 1 within an ulp, so 32 bit samples only come close.
            if (depth == 32)
            {
                float value = 0.0f;
                std::memcpy(&value, resized.bytes.data() + i, 4);
                ASSERT_NEAR(value, 0.3f, 1e-6f) << width << " x " << height << ", byte " << i;
            }
            else
            {
                ASSERT_EQ(std::memcmp(resized.bytes.data() + i, flat.bytes.data(), sample), 0)
                    << width << " x " << height << ", byte " << i;
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    FiltersAndDepths, LayerResizeFilterTest,
    testing::Combine(
        testing::Values(ffpsd::ResampleFilter::kNearest, ffpsd::ResampleFilter::kBicubic), testing::Values<std::uint16_t>(8, 16, 32)));

TEST(LayerResizeTest, NearestDoublesEveryPixel)
{
    const ffpsd::Image image = Pattern(2, 2, 1); // 3, 10 / 17, 24

    const ffpsd::Image resized = Resized(image, 4, 4, ffpsd::ResampleFilter::kNearest);

    const std::vector<std::uint8_t> expected = {3, 3, 10, 10, 3, 3, 10, 10, 17, 17, 24, 24, 17, 17, 24, 24};
    EXPECT_EQ(resized.bytes, expected);
}

TEST(LayerResizeTest, NearestTakesThePixelUnderEachCenter)
{
    // Output pixel x covers source (2x + 1) * in / (2 * out): 3 -> 2 picks 0 and 2, 4 -> 2 picks 1 and 3.
    const ffpsd::Image row = Pattern(3, 1, 1);    // 3, 10, 17
    const ffpsd::Image square = Pattern(4, 4, 1); // 3, 10, 17, 24 / 31, ...

    EXPECT_EQ(Resized(row, 2, 1, ffpsd::ResampleFilter::kNearest).bytes, (std::vector<std::uint8_t>{3, 17}));
    EXPECT_EQ(
        Resized(square, 2, 2, ffpsd::ResampleFilter::kNearest).bytes,
        (std::vector<std::uint8_t>{square.bytes[1 * 4 + 1], square.bytes[1 * 4 + 3], square.bytes[3 * 4 + 1], square.bytes[3 * 4 + 3]}));
}

TEST(LayerResizeTest, BicubicKeepsARampStraight)
{
    // Catmull-Rom keeps a line straight: away from the edges, 8 -> 16 turns the ramp 4i into 2x - 1.
    const ffpsd::Image resized = Resized(Ramp(8, 3), 16, 3);

    for (std::size_t y = 0; y < 3; ++y)
        for (std::size_t x = 4; x < 12; ++x)
            EXPECT_EQ(resized.bytes[y * 16 + x], 2 * x - 1) << "x " << x;
}

TEST(LayerResizeTest, BicubicShrinksByAveragingWhatIsUnder)
{
    // A 2 x 1 image into 1 x 1: both pixels are equally near the center, so each weighs half.
    ffpsd::Image pair = Pattern(2, 1, 1);
    pair.bytes = {100, 200};

    EXPECT_EQ(Resized(pair, 1, 1).bytes, (std::vector<std::uint8_t>{150}));
}

TEST(LayerResizeTest, AClearPixelAddsNoColor)
{
    // Opaque red next to clear black: half coverage, and the color stays pure red.
    ffpsd::Image image = Pattern(2, 1, 4);
    image.bytes = {255, 0, 0, 0, 0, 0, 255, 0}; // R, G, B, A planes

    EXPECT_EQ(Resized(image, 1, 1).bytes, (std::vector<std::uint8_t>{255, 0, 0, 128}));
}
