#include "support/test_support.hpp"

#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace ffpsd_test;

namespace
{
    // The pixels of a layer made from image and resized; gray for 1 or 2 planes, RGB for 3 or 4.
    ffpsd::Image Resized(
        const ffpsd::Image& image, std::uint32_t width, std::uint32_t height,
        ffpsd::ResampleFilter filter = ffpsd::ResampleFilter::kBicubic)
    {
        const ffpsd::ColorMode color_mode = image.channel_count < 3 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        ffpsd::Document doc = NewDocument(color_mode, image.depth);
        ffpsd::Layer* layer = doc.AddLayer("layer", image);
        layer->Resize(width, height, filter);

        // Only the planes the image had: AddLayer gives one without transparency an opaque plane of it.
        ffpsd::Image pixels = layer->GetPixels();
        pixels.channel_count = image.channel_count;
        pixels.bytes = ffpsd::Bytes(pixels.bytes.data(), pixels.GetSizeBytes());
        return pixels;
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

    // Every sample the same: 77 at 8 bit, 0x4D4D at 16 and 0.3 at 32.
    ffpsd::Image FlatAt(std::uint32_t width, std::uint32_t height, std::uint16_t depth)
    {
        ffpsd::Image image = Pattern(width, height, 1, depth);
        const std::size_t sample = image.GetBytesPerSample();
        const std::uint8_t eight = 77;
        const std::uint16_t sixteen = 0x4D4D;
        const float thirty_two = 0.3f;
        const void* value = depth == 8 ? static_cast<const void*>(&eight) : depth == 16 ? static_cast<const void*>(&sixteen) : &thirty_two;
        for (std::size_t i = 0; i < image.bytes.size(); i += sample)
            std::memcpy(image.bytes.data() + i, value, sample);
        return image;
    }

    // A 10 x 10 document with a 4 x 2 layer at 3, 1 and a 2 x 2 mask over its right half.
    struct Masked
    {
        ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 10, 10);
        ffpsd::Image pixels = Pattern(4, 2, 4);
        ffpsd::Image mask = Pattern(2, 2, 1);
        ffpsd::Layer* layer = nullptr;

        Masked()
        {
            layer = doc.AddLayer("layer", pixels, 3, 1);
            layer->SetMask(mask, 3, 3, 0);
        }
    };
} // namespace

TEST(LayerTransformTest, SetPositionAndResizeKeepThePixelsFromTheTopLeftCorner)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("moved", Pattern(3, 2, 4), 1, 1);
    doc.SetHasRealMergedData(true);

    layer->SetPosition(-4, 7);
    ExpectRect(layer->GetBounds(), -4, 7, -2, 10);
    EXPECT_EQ(layer->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());

    // Bounds past 32 bits are refused, and the layer stays; the same top with a new left is still a move.
    EXPECT_THROW(layer->SetPosition(-4, std::numeric_limits<std::int32_t>::max() - 1), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().left, 7);

    layer->SetPosition(2, 5);
    layer->Resize(7, 5);
    ExpectRect(layer->GetBounds(), 2, 5, 7, 12);
    EXPECT_EQ(layer->GetPixels().channel_count, 4u);

    ffpsd::Document deep = NewDocument(ffpsd::ColorMode::kRgb, 16);
    ffpsd::Layer* deep_layer = deep.AddLayer("deep", Pattern(4, 3, 4, 16));
    deep_layer->Resize(9, 2);
    const ffpsd::Image expected = deep_layer->GetPixels();
    const ffpsd::Document back = RoundTrip(deep);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetBounds().GetWidth(), 9);
    EXPECT_EQ(Pixels(back, 0), expected.bytes);
}

TEST(LayerTransformTest, ATurnKeepsTheCenterAndAFlipMirrorsInPlaceMaskIncluded)
{
    Masked turned;
    turned.layer->Rotate(ffpsd::Rotation::k90);
    ExpectRect(turned.layer->GetBounds(), 2, 2, 6, 4);
    EXPECT_EQ(turned.layer->GetPixels().bytes, Clockwise(turned.pixels).bytes);

    // The right half of the mask turns into the bottom half.
    ExpectRect(turned.layer->GetMask()->bounds, 4, 2, 6, 4);
    EXPECT_EQ(turned.layer->GetMask()->image.bytes, Clockwise(turned.mask).bytes);
    EXPECT_FALSE(turned.doc.HasRealMergedData());

    turned.layer->Rotate(ffpsd::Rotation::k180);
    turned.layer->Rotate(ffpsd::Rotation::k90);
    ExpectRect(turned.layer->GetBounds(), 3, 1, 5, 5);
    EXPECT_EQ(turned.layer->GetPixels().bytes, turned.pixels.bytes);
    EXPECT_EQ(turned.layer->GetMask()->image.bytes, turned.mask.bytes);

    Masked flipped;
    flipped.layer->Flip(ffpsd::FlipDirection::kHorizontal);
    ExpectRect(flipped.layer->GetBounds(), 3, 1, 5, 5);
    EXPECT_EQ(flipped.layer->GetPixels().bytes, Mirrored(flipped.pixels).bytes);
    ExpectRect(flipped.layer->GetMask()->bounds, 3, 1, 5, 3);
    EXPECT_EQ(Pixels(RoundTrip(flipped.doc), 0), Mirrored(flipped.pixels).bytes);

    flipped.layer->Flip(ffpsd::FlipDirection::kVertical);
    flipped.layer->Rotate(ffpsd::Rotation::k180);
    EXPECT_EQ(flipped.layer->GetPixels().bytes, flipped.pixels.bytes);

    // An odd side difference: the center is 1.5, 1; turned, the corner would be 0.5, -0.5, so it lands half a pixel up and left.
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* odd = doc.AddLayer("layer", Pattern(3, 2, 4));
    odd->Rotate(ffpsd::Rotation::k90);
    ExpectRect(odd->GetBounds(), -1, 0, 2, 2);
}

TEST(LayerTransformTest, FarFromTheCanvasTheEdgesStayExact)
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

TEST(LayerTransformTest, WhatCannotMoveResizeOrTurnIsRefused)
{
    ffpsd::Document rgb = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Layer* background = rgb.GetLayerByIndex(0);
    EXPECT_THROW(background->SetPosition(1, 0), std::logic_error);
    EXPECT_THROW(background->Resize(10, 10), std::logic_error);
    background->SetPosition(0, 0); // where it is already

    ffpsd::Document levels = ffpsd::Document::Open(kRgbLevelsPsd);
    EXPECT_THROW(levels.GetLayerByIndex(1)->Resize(10, 10), std::logic_error);
    EXPECT_THROW(levels.GetLayerByIndex(1)->SetPosition(3, 3), std::logic_error);

    ffpsd::Document masks = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_THROW(masks.GetLayerByIndex(0)->Rotate(ffpsd::Rotation::k90), std::logic_error);
    EXPECT_THROW(masks.GetLayerByIndex(3)->Flip(ffpsd::FlipDirection::kHorizontal), std::logic_error);

    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* empty = doc.AddLayer("empty");
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(3, 2, 4));
    EXPECT_THROW(empty->Resize(4, 4), std::logic_error);
    EXPECT_THROW(layer->Resize(0, 4), std::invalid_argument);
    EXPECT_THROW(layer->Resize(30001, 4), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().GetWidth(), 3);

    ffpsd::Layer* text = doc.AddLayer("text", Pattern(2, 2, 3));
    text->SetTaggedBlock(Block("TySh", {0}));
    EXPECT_THROW(text->Rotate(ffpsd::Rotation::k180), std::logic_error);
    EXPECT_THROW(text->SetPosition(1, 1), std::logic_error);

    SetSectionDivider(*layer, 3);
    EXPECT_THROW(layer->Resize(4, 4), std::logic_error);
}

// The pixels Resize computes; expected values are worked out by hand from the filter definitions.

TEST(LayerTransformTest, EveryFilterAndDepthKeepsFlatImagesFlat)
{
    for (const ffpsd::ResampleFilter filter : {ffpsd::ResampleFilter::kNearest, ffpsd::ResampleFilter::kBicubic})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}, std::uint16_t{32}})
        {
            const ffpsd::Image image = Pattern(5, 3, 4, depth);
            EXPECT_EQ(Resized(image, 5, 3, filter).bytes, image.bytes) << depth << " bit, the same size";

            const ffpsd::Image flat = FlatAt(6, 4, depth);
            const std::size_t sample = flat.GetBytesPerSample();
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
                            << depth << " bit, " << width << " x " << height << ", byte " << i;
                    }
                }
            }
        }
    }
}

TEST(LayerTransformTest, NearestTakesThePixelUnderEachCenterAndBicubicAverages)
{
    const ffpsd::Image doubled = Resized(Pattern(2, 2, 1), 4, 4, ffpsd::ResampleFilter::kNearest); // 3, 10 / 17, 24
    EXPECT_EQ(doubled.bytes, (ffpsd::Bytes{3, 3, 10, 10, 3, 3, 10, 10, 17, 17, 24, 24, 17, 17, 24, 24}));

    // Output pixel x covers source (2x + 1) * in / (2 * out): 3 -> 2 picks 0 and 2, 4 -> 2 picks 1 and 3.
    const ffpsd::Image row = Pattern(3, 1, 1);    // 3, 10, 17
    const ffpsd::Image square = Pattern(4, 4, 1); // 3, 10, 17, 24 / 31, ...
    EXPECT_EQ(Resized(row, 2, 1, ffpsd::ResampleFilter::kNearest).bytes, (ffpsd::Bytes{3, 17}));
    EXPECT_EQ(
        Resized(square, 2, 2, ffpsd::ResampleFilter::kNearest).bytes,
        (ffpsd::Bytes{square.bytes[1 * 4 + 1], square.bytes[1 * 4 + 3], square.bytes[3 * 4 + 1], square.bytes[3 * 4 + 3]}));

    // Catmull-Rom keeps a line straight: away from the edges, 8 -> 16 turns the ramp 4i into 2x - 1.
    const ffpsd::Image resized = Resized(Ramp(8, 3), 16, 3);
    for (std::size_t y = 0; y < 3; ++y)
        for (std::size_t x = 4; x < 12; ++x)
            EXPECT_EQ(resized.bytes[y * 16 + x], 2 * x - 1) << "x " << x;

    // A 2 x 1 image into 1 x 1: both pixels are equally near the center, so each weighs half.
    EXPECT_EQ(Resized(Planes(2, 1, 1, {100, 200}), 1, 1).bytes, (ffpsd::Bytes{150}));

    // Opaque red next to clear black: half coverage, and the color stays pure red.
    EXPECT_EQ(Resized(Planes(2, 1, 4, {255, 0, 0, 0, 0, 0, 255, 0}), 1, 1).bytes, (ffpsd::Bytes{255, 0, 0, 128}));
}
