#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(LayerPixelsTest, AddLayerKeepsThePlanesWhereTheyWerePut)
{
    ffpsd::Document doc = NewDocument();

    // Without transparency of its own an ordinary layer gets an opaque one, as Photoshop gives it.
    const ffpsd::Image opaque = doc.AddLayer("rgb", Pattern(3, 2, 3))->GetPixels();
    EXPECT_EQ(opaque.channel_count, 4u);
    EXPECT_EQ(opaque.color_mode, ffpsd::ColorMode::kRgb);
    EXPECT_EQ(opaque.bytes, WithOpaqueAlpha(Pattern(3, 2, 3)).bytes);

    const ffpsd::Layer* transparent = doc.AddLayer("off canvas", Pattern(3, 2, 4), -5, 7);
    EXPECT_EQ(transparent->GetPixels().channel_count, 4u);
    EXPECT_EQ(transparent->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    ExpectRect(transparent->GetBounds(), -5, 7, -3, 10);

    // A view borrows a buffer that is no Image.
    const ffpsd::Image pattern = Pattern(3, 2, 4);
    const std::vector<std::uint8_t> planes(pattern.bytes.begin(), pattern.bytes.end());
    const ffpsd::ImageView view(3, 2, 4, 8, ffpsd::ColorMode::kRgb, planes.data(), planes.size());
    EXPECT_EQ(doc.AddLayer("planar", view)->GetPixels().bytes, pattern.bytes);

    const ffpsd::Layer* empty = doc.AddLayer("empty", ffpsd::Image(), 2, 1);
    ExpectRect(empty->GetBounds(), 2, 1, 2, 1);
    EXPECT_TRUE(empty->GetPixels().bytes.empty());

    // 16 bit samples stay in native order.
    ffpsd::Document deep = NewDocument(ffpsd::ColorMode::kRgb, 16);
    EXPECT_EQ(deep.AddLayer("deep", Pattern(2, 1, 3, 16))->GetPixels().bytes, WithOpaqueAlpha(Pattern(2, 1, 3, 16)).bytes);
}

TEST(LayerPixelsTest, AddLayerRefusesWhatDoesNotFit)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Image image = Pattern(2, 2, 3);

    EXPECT_THROW(doc.AddLayer("gray", Pattern(2, 2, 1)), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("five", Pattern(2, 2, 5)), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("deep", Pattern(2, 2, 3, 16)), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer("lab", Pattern(2, 2, 3, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
    ffpsd::ImageView short_view = image;
    short_view.size -= 1;
    EXPECT_THROW(doc.AddLayer("short", short_view), std::invalid_argument);
    ffpsd::ImageView wide = image;
    wide.width = 30001;
    EXPECT_THROW(doc.AddLayer("wide", wide), std::invalid_argument);
    EXPECT_EQ(doc.GetLayerCount(), 0u);

    // Photoshop keeps no layers in these modes, whatever the image.
    for (const ffpsd::ColorMode mode : {ffpsd::ColorMode::kBitmap, ffpsd::ColorMode::kIndexed})
    {
        ffpsd::Document none(4, 3, mode, mode == ffpsd::ColorMode::kBitmap ? 1 : 8);
        EXPECT_THROW(none.AddLayer("none"), std::logic_error);
        EXPECT_THROW(none.AddBackgroundLayer("none", Pattern(4, 3, 1)), std::logic_error);
        EXPECT_THROW(none.AddAdjustmentLayer<ffpsd::LevelsInfo>("none"), std::logic_error);
        EXPECT_EQ(none.GetLayerCount(), 0u);
    }
}

TEST(LayerPixelsTest, SetPixelsReplacesColorAndTransparencyOnly)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Layer* layer = doc.GetLayerByIndex(1);
    const ffpsd::Image before = layer->GetPixels();
    const ffpsd::Rect bounds = layer->GetBounds();

    // A wrong image leaves the layer as it was.
    EXPECT_THROW(layer->SetPixels(Pattern(3, 2, 1)), std::invalid_argument);
    EXPECT_THROW(layer->SetPixels(Pattern(3, 2, 4, 16)), std::invalid_argument);
    EXPECT_THROW(layer->SetPixels(Pattern(3, 2, 4, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
    EXPECT_EQ(layer->GetBounds().GetWidth(), 1890);
    EXPECT_EQ(layer->GetPixels().bytes, before.bytes);

    layer->SetPixels(before);
    EXPECT_EQ(layer->GetPixels().bytes, before.bytes);
    EXPECT_EQ(layer->GetBounds().right, bounds.right);
    EXPECT_EQ(layer->GetBounds().bottom, bounds.bottom);
    EXPECT_FALSE(doc.HasRealMergedData());

    // A new size keeps the top left corner.
    ffpsd::Document made = NewDocument();
    ffpsd::Layer* moved = made.AddLayer("moved", Pattern(4, 3, 3), 5, 6);
    moved->SetPixels(Pattern(2, 1, 4));
    ExpectRect(moved->GetBounds(), 5, 6, 6, 8);
    EXPECT_EQ(moved->GetPixels().bytes, Pattern(2, 1, 4).bytes);

    // Photoshop's Levels layer has a layer mask channel next to its empty ones, and keeps it.
    ffpsd::Document levels = ffpsd::Document::Open(kRgbLevelsPsd);
    levels.SetHasRealMergedData(false);
    const std::vector<std::uint8_t> saved = levels.Save();
    levels.GetLayerByIndex(1)->SetPixels(ffpsd::Image());
    EXPECT_EQ(levels.Save(), saved);
}

TEST(LayerPixelsTest, SetPositionMovesThePixelsAsTheyAre)
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
}

TEST(LayerPixelsTest, BytesIntoTheCallersMemoryMatchGetPixels)
{
    const auto check = [](const ffpsd::Document& doc) {
        for (std::size_t i = 0; i < doc.GetLayerCount(); ++i)
        {
            const ffpsd::Layer& layer = *doc.GetLayerByIndex(i);
            const ffpsd::Image image = layer.GetPixels();
            const ffpsd::ImageInfo info = layer.GetPixelsInfo();
            EXPECT_EQ(info.width, image.width) << i;
            EXPECT_EQ(info.height, image.height) << i;
            EXPECT_EQ(info.channel_count, image.channel_count) << i;
            EXPECT_EQ(info.depth, image.depth) << i;
            EXPECT_EQ(info.color_mode, image.color_mode) << i;

            // Filled with a pattern first, so a byte left unwritten would show.
            ffpsd::Bytes bytes(info.GetSizeBytes(), 0xAB);
            layer.GetPixelsBytes(bytes.data(), bytes.size());
            EXPECT_EQ(bytes, image.bytes) << i;
        }
    };

    // Photoshop's RLE channels, then the same written raw.
    for (const std::string& path : {kRgbPsd, kGrayscalePsd, kRgbMasksPsd})
    {
        const ffpsd::Document doc = ffpsd::Document::Open(path);
        check(doc);
        check(ffpsd::Document::Parse(doc.Save(ffpsd::Compression::kRaw)));
    }

    // Exactly the size, and a view over them is the Image's view.
    const ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);
    const ffpsd::Layer& layer = *doc.GetLayerByIndex(1);
    const std::size_t size = layer.GetPixelsInfo().GetSizeBytes();
    std::vector<std::uint8_t> bytes(size + 1);
    EXPECT_THROW(layer.GetPixelsBytes(bytes.data(), size - 1), std::invalid_argument);
    EXPECT_THROW(layer.GetPixelsBytes(bytes.data(), size + 1), std::invalid_argument);
    EXPECT_THROW(layer.GetPixelsBytes(nullptr, size), std::invalid_argument);
    layer.GetPixelsBytes(bytes.data(), size);
    const ffpsd::ImageView view(layer.GetPixelsInfo(), bytes.data(), size);
    EXPECT_EQ(view.GetSizeBytes(), size);
    EXPECT_EQ(view.channel_count, 4u);
}
