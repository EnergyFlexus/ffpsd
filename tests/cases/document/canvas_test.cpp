#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

using namespace ffpsd_test;

namespace
{
    // An 8 bit image on a white one of the given size, cut where it leaves it.
    ffpsd::Image Placed(const ffpsd::Image& image, std::uint32_t width, std::uint32_t height, std::int64_t top, std::int64_t left)
    {
        ffpsd::Image result = image;
        result.width = width;
        result.height = height;
        result.bytes = ffpsd::Bytes(result.GetSizeBytes(), 255);
        for (std::size_t c = 0; c < image.channel_count; ++c)
        {
            for (std::int64_t y = 0; y < height; ++y)
            {
                for (std::int64_t x = 0; x < width; ++x)
                {
                    const std::int64_t from_y = y - top;
                    const std::int64_t from_x = x - left;
                    if (from_y >= 0 && from_y < image.height && from_x >= 0 && from_x < image.width)
                        result.bytes[(c * height + y) * width + x] = image.bytes[(c * image.height + from_y) * image.width + from_x];
                }
            }
        }
        return result;
    }

    // Every 8 bit pixel twice as wide and tall.
    ffpsd::Image Doubled(const ffpsd::Image& image)
    {
        ffpsd::Image result = image;
        result.width *= 2;
        result.height *= 2;
        result.bytes = ffpsd::Bytes(result.GetSizeBytes());
        for (std::size_t c = 0; c < image.channel_count; ++c)
        {
            for (std::size_t y = 0; y < result.height; ++y)
            {
                for (std::size_t x = 0; x < result.width; ++x)
                    result.bytes[(c * result.height + y) * result.width + x] =
                        image.bytes[(c * image.height + y / 2) * image.width + x / 2];
            }
        }
        return result;
    }

    const ffpsd::Image kPicture = Pattern(4, 3, 3);
    const ffpsd::Image kLayer = Pattern(2, 2, 4);

    // The picture as the background and the composite, kLayer at 1, 1.
    ffpsd::Document Stack()
    {
        ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, kPicture.width, kPicture.height);
        doc.AddBackgroundLayer("background", kPicture);
        doc.AddLayer("layer", kLayer, 1, 1);
        doc.SetMergedImage(kPicture);
        return doc;
    }
} // namespace

TEST(DocumentCanvasTest, ResizeCanvasMovesLayersAndPadsOrCutsTheBackground)
{
    ffpsd::Document grown = Stack();
    grown.ResizeCanvas(6, 5);
    EXPECT_EQ(grown.GetWidth(), 6u);
    EXPECT_EQ(grown.GetHeight(), 5u);
    ExpectRect(grown.GetLayerByIndex(0)->GetBounds(), 0, 0, 5, 6);
    ExpectRect(grown.GetLayerByIndex(1)->GetBounds(), 2, 2, 4, 4);
    EXPECT_EQ(Pixels(grown, 0), Placed(kPicture, 6, 5, 1, 1).bytes);
    EXPECT_EQ(grown.GetMergedImage().bytes, Placed(kPicture, 6, 5, 1, 1).bytes);
    EXPECT_FALSE(grown.HasRealMergedData());

    const ffpsd::Document back = RoundTrip(grown);
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    EXPECT_EQ(Pixels(back, 0), Placed(kPicture, 6, 5, 1, 1).bytes);
    EXPECT_EQ(Pixels(back, 1), kLayer.bytes);
    ExpectRect(back.GetLayerByIndex(1)->GetBounds(), 2, 2, 4, 4);

    // The background is cut on the anchor's side; an ordinary layer keeps its pixels past the edge.
    ffpsd::Document shrunk = Stack();
    shrunk.ResizeCanvas(2, 2, ffpsd::Anchor::kBottomRight);
    EXPECT_EQ(Pixels(shrunk, 0), Placed(kPicture, 2, 2, -1, -2).bytes);
    EXPECT_EQ(shrunk.GetMergedImage().bytes, Placed(kPicture, 2, 2, -1, -2).bytes);
    ExpectRect(shrunk.GetLayerByIndex(1)->GetBounds(), 0, -1, 2, 1);
    EXPECT_EQ(Pixels(shrunk, 1), kLayer.bytes);

    // The odd row goes below both ways, as in Photoshop: 20 to 39 adds 9 rows above, 39 to 20 takes the 21st row to the 12th.
    ffpsd::Document odd_grown = Stack();
    odd_grown.ResizeCanvas(4, 4);
    EXPECT_EQ(odd_grown.GetMergedImage().bytes, Placed(kPicture, 4, 4, 0, 0).bytes);
    ffpsd::Document odd_cut = Stack();
    odd_cut.ResizeCanvas(4, 2);
    EXPECT_EQ(odd_cut.GetMergedImage().bytes, Placed(kPicture, 4, 2, 0, 0).bytes);

    // Without layers the composite is the picture itself, so it stays real; its alpha channels come along empty.
    ffpsd::Document alpha = NewDocument(ffpsd::ColorMode::kRgb, 8, 4, 3);
    const ffpsd::Image five = Pattern(4, 3, 5);
    alpha.SetMergedImage(five);
    alpha.ResizeCanvas(4, 4, ffpsd::Anchor::kTop);
    EXPECT_TRUE(alpha.HasRealMergedData());
    ffpsd::Image expected = Placed(five, 4, 4, 0, 0);
    for (std::size_t c = 3; c < 5; ++c)
        for (std::size_t x = 0; x < 4; ++x)
            expected.bytes[(c * 4 + 3) * 4 + x] = 0;
    EXPECT_EQ(alpha.GetChannelCount(), 5u);
    EXPECT_EQ(alpha.GetMergedImage().bytes, expected.bytes);
    alpha.Resize(2, 2);
    EXPECT_EQ(alpha.GetMergedImage().channel_count, 5u);
}

TEST(DocumentCanvasTest, ResizeScalesEveryLayerItsPositionAndTheComposite)
{
    ffpsd::Document doc = Stack();

    doc.Resize(8, 6, ffpsd::ResampleFilter::kNearest);

    EXPECT_EQ(doc.GetWidth(), 8u);
    EXPECT_EQ(doc.GetHeight(), 6u);
    ExpectRect(doc.GetLayerByIndex(0)->GetBounds(), 0, 0, 6, 8);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 2, 2, 6, 6);
    EXPECT_EQ(Pixels(doc, 0), Doubled(kPicture).bytes);
    EXPECT_EQ(Pixels(doc, 1), Doubled(kLayer).bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, Doubled(kPicture).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());
    EXPECT_EQ(Pixels(RoundTrip(doc), 1), Doubled(kLayer).bytes);
}

TEST(DocumentCanvasTest, TurnsAndFlipsMoveEverythingExactly)
{
    ffpsd::Document doc = Stack();
    ffpsd::ResolutionInfo resolution;
    resolution.vertical = 144.0;
    doc.SetResolutionInfo(resolution);

    doc.RotateCanvas(ffpsd::Rotation::k90);

    EXPECT_EQ(doc.GetWidth(), 3u);
    EXPECT_EQ(doc.GetHeight(), 4u);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 144.0);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().vertical, 72.0);
    ExpectRect(doc.GetLayerByIndex(0)->GetBounds(), 0, 0, 4, 3);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 1, 0, 3, 2);
    EXPECT_EQ(Pixels(doc, 0), Clockwise(kPicture).bytes);
    EXPECT_EQ(Pixels(doc, 1), Clockwise(kLayer).bytes);

    // Nothing is resampled, so the composite stays the real picture.
    EXPECT_EQ(doc.GetMergedImage().bytes, Clockwise(kPicture).bytes);
    EXPECT_TRUE(doc.HasRealMergedData());
    const ffpsd::Document back = RoundTrip(doc);
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    EXPECT_EQ(back.GetMergedImage().bytes, Clockwise(kPicture).bytes);

    doc.RotateCanvas(ffpsd::Rotation::k270);
    EXPECT_EQ(doc.GetWidth(), 4u);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 1, 1, 3, 3);
    EXPECT_EQ(Pixels(doc, 0), kPicture.bytes);

    // A flip mirrors layers across the canvas; with a half turn they come back where they were.
    doc.GetLayerByIndex(1)->SetPosition(0, 0);
    doc.FlipCanvas(ffpsd::FlipDirection::kHorizontal);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 0, 2, 2, 4);
    doc.FlipCanvas(ffpsd::FlipDirection::kVertical);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 1, 2, 3, 4);
    doc.RotateCanvas(ffpsd::Rotation::k180);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 0, 0, 2, 2);
    EXPECT_EQ(Pixels(doc, 0), kPicture.bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, kPicture.bytes);
}

TEST(DocumentCanvasTest, PhotoshopsMasksFollowAndVectorMasksOnlyScale)
{
    // Layer 3's vector mask is in fractions of the canvas, which a new canvas or a turn would leave wrong.
    ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_THROW(doc.ResizeCanvas(2000, 1500), std::logic_error);
    EXPECT_THROW(doc.RotateCanvas(ffpsd::Rotation::k90), std::logic_error);
    EXPECT_EQ(doc.GetWidth(), 1890u);

    doc.Resize(945, 709);
    const ffpsd::Document scaled = RoundTrip(doc);
    EXPECT_EQ(scaled.GetWidth(), 945u);
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(Pixels(scaled, i), Pixels(doc, i));

    const ffpsd::Document original = ffpsd::Document::Open(kRgbMasksPsd);
    const ffpsd::Rect masked = original.GetLayerByIndex(2)->GetBounds();
    ffpsd::Document moved = ffpsd::Document::Open(kRgbMasksPsd);
    moved.RemoveLayer(3);
    moved.ResizeCanvas(1900, 1437, ffpsd::Anchor::kBottomRight);
    const ffpsd::Document back = RoundTrip(moved);
    ExpectRect(back.GetLayerByIndex(2)->GetBounds(), masked.top + 20, masked.left + 10, masked.bottom + 20, masked.right + 10);
    EXPECT_EQ(Pixels(back, 2), Pixels(original, 2));

    ffpsd::Document turned = ffpsd::Document::Open(kRgbMasksPsd);
    turned.RemoveLayer(3);
    const ffpsd::LayerMask mask = *turned.GetLayerByIndex(2)->GetMask();
    turned.RotateCanvas(ffpsd::Rotation::k90);
    const ffpsd::LayerMask turned_mask = *RoundTrip(turned).GetLayerByIndex(2)->GetMask();
    ExpectRect(turned_mask.bounds, 935, 0, 1890, 1417);
    EXPECT_EQ(turned_mask.image.bytes, Clockwise(mask.image).bytes);
}

TEST(DocumentCanvasTest, WhatCannotChangeStopsItBeforeAnythingChanges)
{
    // Text and smart objects stop both; a vector mask only a new canvas.
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("text", Pattern(2, 2, 3), 1, 1);
    layer->SetTaggedBlock(Block("TySh", {0}));
    EXPECT_THROW(doc.ResizeCanvas(8, 6), std::logic_error);
    EXPECT_THROW(doc.Resize(8, 6), std::logic_error);
    EXPECT_EQ(doc.GetWidth(), 4u);
    ExpectRect(layer->GetBounds(), 1, 1, 3, 3);

    layer->RemoveTaggedBlock(Fourcc("TySh"));
    layer->SetTaggedBlock(Block("vmsk", {0}));
    EXPECT_THROW(doc.ResizeCanvas(8, 6), std::logic_error);
    doc.Resize(8, 6);
    ExpectRect(layer->GetBounds(), 2, 2, 6, 6);

    // The size stays within the format.
    ffpsd::Document sized = NewDocument();
    sized.ResizeCanvas(30000, 3);
    EXPECT_THROW(sized.ResizeCanvas(30001, 3), std::invalid_argument);
    EXPECT_THROW(sized.Resize(4, 0), std::invalid_argument);
    EXPECT_EQ(sized.GetWidth(), 30000u);

    sized.SetPsb(true);
    sized.ResizeCanvas(300000, 300000);
    EXPECT_THROW(sized.ResizeCanvas(300001, 3), std::invalid_argument);
    EXPECT_EQ(sized.GetWidth(), 300000u);
}
