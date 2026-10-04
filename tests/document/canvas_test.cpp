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
        result.bytes.assign(result.GetSizeBytes(), 255);
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
        result.bytes.resize(result.GetSizeBytes());
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

    ffpsd::Document Stack(const ffpsd::Image& picture)
    {
        ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, picture.width, picture.height);
        doc.AddBackgroundLayer("background", picture);
        doc.AddLayer("layer", Pattern(2, 2, 4), 1, 1);
        doc.SetMergedImage(picture);
        return doc;
    }
} // namespace

TEST(DocumentCanvasTest, AGrownCanvasMovesLayersAndPadsTheBackgroundWithWhite)
{
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document doc = Stack(picture);

    doc.ResizeCanvas(6, 5);

    EXPECT_EQ(doc.GetWidth(), 6u);
    EXPECT_EQ(doc.GetHeight(), 5u);
    ExpectRect(doc.GetLayerByIndex(0)->GetBounds(), 0, 0, 5, 6);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 2, 2, 4, 4);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, Placed(picture, 6, 5, 1, 1).bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, Placed(picture, 6, 5, 1, 1).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Placed(picture, 6, 5, 1, 1).bytes);
    EXPECT_EQ(back.GetLayerByIndex(1)->GetPixels().bytes, Pattern(2, 2, 4).bytes);
    ExpectRect(back.GetLayerByIndex(1)->GetBounds(), 2, 2, 4, 4);
}

TEST(DocumentCanvasTest, AShrunkCanvasCutsTheBackgroundOnTheAnchorsSide)
{
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document doc = Stack(picture);

    doc.ResizeCanvas(2, 2, ffpsd::Anchor::kBottomRight);

    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, Placed(picture, 2, 2, -1, -2).bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, Placed(picture, 2, 2, -1, -2).bytes);

    // An ordinary layer keeps its pixels past the edge.
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 0, -1, 2, 1);
    EXPECT_EQ(doc.GetLayerByIndex(1)->GetPixels().bytes, Pattern(2, 2, 4).bytes);
}

TEST(DocumentCanvasTest, AnOddChangeAroundTheCenterGoesToTheBottom)
{
    // The odd row goes below both ways, as in Photoshop: 20 to 39 adds 9 rows above, 39 to 20 takes the 21st row to the 12th.
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document grown = Stack(picture);
    grown.ResizeCanvas(4, 4);
    EXPECT_EQ(grown.GetMergedImage().bytes, Placed(picture, 4, 4, 0, 0).bytes);

    ffpsd::Document cut = Stack(picture);
    cut.ResizeCanvas(4, 2);
    EXPECT_EQ(cut.GetMergedImage().bytes, Placed(picture, 4, 2, 0, 0).bytes);
}

TEST(DocumentCanvasTest, ResizeScalesEveryLayerItsPositionAndTheComposite)
{
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document doc = Stack(picture);

    doc.Resize(8, 6, ffpsd::ResampleFilter::kNearest);

    EXPECT_EQ(doc.GetWidth(), 8u);
    EXPECT_EQ(doc.GetHeight(), 6u);
    ExpectRect(doc.GetLayerByIndex(0)->GetBounds(), 0, 0, 6, 8);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 2, 2, 6, 6);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, Doubled(picture).bytes);
    EXPECT_EQ(doc.GetLayerByIndex(1)->GetPixels().bytes, Doubled(Pattern(2, 2, 4)).bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, Doubled(picture).bytes);
    EXPECT_FALSE(doc.HasRealMergedData());

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    EXPECT_EQ(back.GetLayerByIndex(1)->GetPixels().bytes, Doubled(Pattern(2, 2, 4)).bytes);
}

TEST(DocumentCanvasTest, AlphaChannelsComeAlongEmptyWhereTheCanvasGrows)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 4, 3);
    const ffpsd::Image five = Pattern(4, 3, 5);
    doc.SetMergedImage(five);

    doc.ResizeCanvas(4, 4, ffpsd::Anchor::kTop);

    // Without layers the composite is the picture itself, so it stays real.
    EXPECT_TRUE(doc.HasRealMergedData());
    ffpsd::Image expected = Placed(five, 4, 4, 0, 0);
    for (std::size_t c = 3; c < 5; ++c)
        for (std::size_t x = 0; x < 4; ++x)
            expected.bytes[(c * 4 + 3) * 4 + x] = 0;
    EXPECT_EQ(doc.GetChannelCount(), 5u);
    EXPECT_EQ(doc.GetMergedImage().bytes, expected.bytes);

    doc.Resize(2, 2);
    EXPECT_EQ(doc.GetMergedImage().channel_count, 5u);
}

TEST(DocumentCanvasTest, MasksFollowAndVectorMasksOnlyScale)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    const ffpsd::Rect masked = doc.GetLayerByIndex(2)->GetBounds();

    // Layer 3's vector mask is in fractions of the canvas, which a new canvas would leave wrong.
    EXPECT_THROW(doc.ResizeCanvas(2000, 1500), std::logic_error);
    EXPECT_EQ(doc.GetWidth(), 1890u);

    doc.Resize(945, 709);
    const ffpsd::Document scaled = ffpsd::Document::Parse(doc.Save());
    EXPECT_EQ(scaled.GetWidth(), 945u);
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(scaled.GetLayerByIndex(i)->GetPixels().bytes, doc.GetLayerByIndex(i)->GetPixels().bytes);

    ffpsd::Document moved = ffpsd::Document::Open(kRgbMasksPsd);
    moved.RemoveLayer(3);
    moved.ResizeCanvas(1900, 1437, ffpsd::Anchor::kBottomRight);
    const ffpsd::Document back = ffpsd::Document::Parse(moved.Save());
    ExpectRect(back.GetLayerByIndex(2)->GetBounds(), masked.top + 20, masked.left + 10, masked.bottom + 20, masked.right + 10);
    EXPECT_EQ(back.GetLayerByIndex(2)->GetPixels().bytes, ffpsd::Document::Open(kRgbMasksPsd).GetLayerByIndex(2)->GetPixels().bytes);
}

TEST(DocumentCanvasTest, TextAndSmartObjectsStopItBeforeAnythingChanges)
{
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
}

TEST(DocumentCanvasTest, TheSizeStaysWithinTheFormat)
{
    ffpsd::Document doc = NewDocument();
    doc.ResizeCanvas(30000, 3);
    EXPECT_THROW(doc.ResizeCanvas(30001, 3), std::invalid_argument);
    EXPECT_THROW(doc.Resize(4, 0), std::invalid_argument);
    EXPECT_EQ(doc.GetWidth(), 30000u);

    doc.SetPsb(true);
    doc.ResizeCanvas(300000, 300000);
    EXPECT_THROW(doc.ResizeCanvas(300001, 3), std::invalid_argument);
    EXPECT_EQ(doc.GetWidth(), 300000u);
}

TEST(DocumentCanvasTest, AQuarterTurnSwapsTheSidesAndTurnsEverything)
{
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document doc = Stack(picture);
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
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, Clockwise(picture).bytes);
    EXPECT_EQ(doc.GetLayerByIndex(1)->GetPixels().bytes, Clockwise(Pattern(2, 2, 4)).bytes);

    // Nothing is resampled, so the composite stays the real picture.
    EXPECT_EQ(doc.GetMergedImage().bytes, Clockwise(picture).bytes);
    EXPECT_TRUE(doc.HasRealMergedData());

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    EXPECT_EQ(back.GetMergedImage().bytes, Clockwise(picture).bytes);

    doc.RotateCanvas(ffpsd::Rotation::k270);
    EXPECT_EQ(doc.GetWidth(), 4u);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 1, 1, 3, 3);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, picture.bytes);
}

TEST(DocumentCanvasTest, AFlippedCanvasMirrorsLayersAcrossIt)
{
    const ffpsd::Image picture = Pattern(4, 3, 3);
    ffpsd::Document doc = Stack(picture);
    doc.GetLayerByIndex(1)->SetPosition(0, 0);

    doc.FlipCanvas(ffpsd::FlipDirection::kHorizontal);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 0, 2, 2, 4);

    doc.FlipCanvas(ffpsd::FlipDirection::kVertical);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 1, 2, 3, 4);

    doc.RotateCanvas(ffpsd::Rotation::k180);
    ExpectRect(doc.GetLayerByIndex(1)->GetBounds(), 0, 0, 2, 2);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, picture.bytes);
    EXPECT_EQ(doc.GetMergedImage().bytes, picture.bytes);
}

TEST(DocumentCanvasTest, PhotoshopsMasksTurnWithTheCanvas)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbMasksPsd);
    EXPECT_THROW(doc.RotateCanvas(ffpsd::Rotation::k90), std::logic_error);
    EXPECT_EQ(doc.GetWidth(), 1890u);

    doc.RemoveLayer(3);
    const ffpsd::LayerMask mask = *doc.GetLayerByIndex(2)->GetMask();
    doc.RotateCanvas(ffpsd::Rotation::k90);

    const ffpsd::LayerMask turned = *ffpsd::Document::Parse(doc.Save()).GetLayerByIndex(2)->GetMask();
    EXPECT_EQ(turned.bounds.top, 935);
    EXPECT_EQ(turned.bounds.left, 0);
    EXPECT_EQ(turned.bounds.bottom, 1890);
    EXPECT_EQ(turned.bounds.right, 1417);
    EXPECT_EQ(turned.image.bytes, Clockwise(mask.image).bytes);
}
