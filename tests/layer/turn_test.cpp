#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>

using namespace ffpsd_test;

namespace
{
    // An 8 bit image with the pixel at x, y moved to where(x, y, width, height).
    template <class Where> ffpsd::Image Moved(const ffpsd::Image& image, bool swap_sides, const Where& where)
    {
        ffpsd::Image result = image;
        if (swap_sides)
            std::swap(result.width, result.height);
        for (std::uint32_t c = 0; c < image.channel_count; ++c)
        {
            for (std::uint32_t y = 0; y < image.height; ++y)
            {
                for (std::uint32_t x = 0; x < image.width; ++x)
                {
                    const auto [to_x, to_y] = where(x, y, image.width, image.height);
                    result.bytes[(c * result.height + to_y) * result.width + to_x] = image.bytes[(c * image.height + y) * image.width + x];
                }
            }
        }
        return result;
    }

    ffpsd::Image Mirrored(const ffpsd::Image& image)
    {
        return Moved(
            image, false, [](std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t) { return std::pair(w - 1 - x, y); });
    }

} // namespace

TEST(LayerTurnTest, AQuarterTurnKeepsTheCenterAndTurnsTheMask)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 10, 10);
    const ffpsd::Image pixels = Pattern(4, 2, 4);
    const ffpsd::Image mask = Pattern(2, 2, 1);
    ffpsd::Layer* layer = doc.AddLayer("layer", pixels, 3, 1);
    layer->SetMask(mask, 3, 3, 0);

    layer->Rotate(ffpsd::Rotation::k90);

    ExpectRect(layer->GetBounds(), 2, 2, 6, 4);
    EXPECT_EQ(layer->GetPixels().bytes, Clockwise(pixels).bytes);
    // The right half of the mask turns into the bottom half.
    ExpectRect(layer->GetMask()->bounds, 4, 2, 6, 4);
    EXPECT_EQ(layer->GetMask()->image.bytes, Clockwise(mask).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());

    layer->Rotate(ffpsd::Rotation::k180);
    layer->Rotate(ffpsd::Rotation::k90);
    ExpectRect(layer->GetBounds(), 3, 1, 5, 5);
    EXPECT_EQ(layer->GetPixels().bytes, pixels.bytes);
    EXPECT_EQ(layer->GetMask()->image.bytes, mask.bytes);
}

TEST(LayerTurnTest, AFlipMirrorsPixelsAndMaskInPlace)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 10, 10);
    const ffpsd::Image pixels = Pattern(4, 2, 4);
    ffpsd::Layer* layer = doc.AddLayer("layer", pixels, 3, 1);
    layer->SetMask(Pattern(2, 2, 1), 3, 3, 0);

    layer->Flip(ffpsd::FlipDirection::kHorizontal);
    ExpectRect(layer->GetBounds(), 3, 1, 5, 5);
    EXPECT_EQ(layer->GetPixels().bytes, Mirrored(pixels).bytes);
    ExpectRect(layer->GetMask()->bounds, 3, 1, 5, 3);

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Mirrored(pixels).bytes);

    layer->Flip(ffpsd::FlipDirection::kVertical);
    layer->Rotate(ffpsd::Rotation::k180);
    EXPECT_EQ(layer->GetPixels().bytes, pixels.bytes);
}

TEST(LayerTurnTest, AnOddSideDifferenceLandsHalfAPixelUpAndLeft)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(3, 2, 4));

    // The center is 1.5, 1; turned, the corner would be 0.5, -0.5.
    layer->Rotate(ffpsd::Rotation::k90);
    ExpectRect(layer->GetBounds(), -1, 0, 2, 2);
}

TEST(LayerTurnTest, WhatCannotTurnIsRefused)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_THROW(doc.GetLayerByIndex(0)->Rotate(ffpsd::Rotation::k90), std::logic_error);
    EXPECT_THROW(doc.GetLayerByIndex(3)->Flip(ffpsd::FlipDirection::kHorizontal), std::logic_error);

    ffpsd::Document text = NewDocument();
    ffpsd::Layer* layer = text.AddLayer("text", Pattern(2, 2, 3));
    layer->SetTaggedBlock(Block("TySh", {0}));
    EXPECT_THROW(layer->Rotate(ffpsd::Rotation::k180), std::logic_error);
    EXPECT_THROW(layer->SetPosition(1, 1), std::logic_error);
}

TEST(LayerTurnTest, FarFromTheCanvasTheEdgesStayExact)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("far", Pattern(3, 2, 4), 2000000001, -2000000001);

    layer->Resize(7, 5);
    ExpectRect(layer->GetBounds(), 2000000001, -2000000001, 2000000006, -1999999994);

    layer->Rotate(ffpsd::Rotation::k90);
    ExpectRect(layer->GetBounds(), 2000000000, -2000000000, 2000000007, -1999999995);

    layer->SetPosition(-2000000003, 2000000004);
    ExpectRect(layer->GetBounds(), -2000000003, 2000000004, -1999999996, 2000000009);
}
