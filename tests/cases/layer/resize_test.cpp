#include "support/test_support.hpp"

#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

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
} // namespace

TEST(LayerResizeTest, ResizeKeepsTheTopLeftCornerThroughSaving)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("resized", Pattern(3, 2, 4), 2, 5);
    layer->Resize(7, 5);
    ExpectRect(layer->GetBounds(), 2, 5, 7, 12);
    EXPECT_EQ(layer->GetPixels().channel_count, 4u);

    ffpsd::Document deep = NewDocument(ffpsd::ColorMode::kRgb, 16);
    ffpsd::Layer* deep_layer = deep.AddLayer("deep", Pattern(4, 3, 4, 16));
    deep_layer->Resize(9, 2);
    const ffpsd::Image expected = deep_layer->GetPixels();
    const ffpsd::Document back = ffpsd::Document::Parse(deep.Save());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetBounds().GetWidth(), 9);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, expected.bytes);
}

TEST(LayerResizeTest, WhatCannotMoveOrResizeIsRefused)
{
    ffpsd::Document rgb = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Layer* background = rgb.GetLayerByIndex(0);
    EXPECT_THROW(background->SetPosition(1, 0), std::logic_error);
    EXPECT_THROW(background->Resize(10, 10), std::logic_error);
    background->SetPosition(0, 0); // where it is already

    ffpsd::Document levels = ffpsd::Document::Open(kRgbLevelsPsd);
    EXPECT_THROW(levels.GetLayerByIndex(1)->Resize(10, 10), std::logic_error);
    EXPECT_THROW(levels.GetLayerByIndex(1)->SetPosition(3, 3), std::logic_error);

    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* empty = doc.AddLayer("empty");
    ffpsd::Layer* layer = doc.AddLayer("layer", Pattern(3, 2, 4));
    EXPECT_THROW(empty->Resize(4, 4), std::logic_error);
    EXPECT_THROW(layer->Resize(0, 4), std::invalid_argument);
    EXPECT_THROW(layer->Resize(30001, 4), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().GetWidth(), 3);

    SetSectionDivider(*layer, 3);
    EXPECT_THROW(layer->Resize(4, 4), std::logic_error);
}

// The pixels Resize computes; expected values are worked out by hand from the filter definitions.

TEST(LayerResizeTest, EveryFilterAndDepthKeepsFlatImagesFlat)
{
    for (const ffpsd::ResampleFilter filter : {ffpsd::ResampleFilter::kNearest, ffpsd::ResampleFilter::kBicubic})
    {
        for (const std::uint16_t depth : {std::uint16_t{8}, std::uint16_t{16}, std::uint16_t{32}})
        {
            const ffpsd::Image image = Pattern(5, 3, 4, depth);
            EXPECT_EQ(Resized(image, 5, 3, filter).bytes, image.bytes) << depth << " bit, the same size";

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
                            << depth << " bit, " << width << " x " << height << ", byte " << i;
                    }
                }
            }
        }
    }
}

TEST(LayerResizeTest, NearestTakesThePixelUnderEachCenter)
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
}

TEST(LayerResizeTest, BicubicAveragesAndKeepsLinesStraight)
{
    // Catmull-Rom keeps a line straight: away from the edges, 8 -> 16 turns the ramp 4i into 2x - 1.
    const ffpsd::Image resized = Resized(Ramp(8, 3), 16, 3);
    for (std::size_t y = 0; y < 3; ++y)
        for (std::size_t x = 4; x < 12; ++x)
            EXPECT_EQ(resized.bytes[y * 16 + x], 2 * x - 1) << "x " << x;

    // A 2 x 1 image into 1 x 1: both pixels are equally near the center, so each weighs half.
    ffpsd::Image pair = Pattern(2, 1, 1);
    pair.bytes = {100, 200};
    EXPECT_EQ(Resized(pair, 1, 1).bytes, (ffpsd::Bytes{150}));

    // Opaque red next to clear black: half coverage, and the color stays pure red.
    ffpsd::Image image = Pattern(2, 1, 4);
    image.bytes = {255, 0, 0, 0, 0, 0, 255, 0}; // R, G, B, A planes
    EXPECT_EQ(Resized(image, 1, 1).bytes, (ffpsd::Bytes{255, 0, 0, 128}));
}
