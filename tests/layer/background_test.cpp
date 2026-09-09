#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

namespace
{
    // The flags of each record, which the API hides: they follow "8BIMnorm", opacity and clipping.
    std::vector<std::uint8_t> NormalLayerFlags(const std::vector<std::uint8_t>& psd)
    {
        const std::uint8_t marker[] = {'8', 'B', 'I', 'M', 'n', 'o', 'r', 'm'};
        std::vector<std::uint8_t> flags;
        auto at = psd.begin();
        while ((at = std::search(at, psd.end(), std::begin(marker), std::end(marker))) != psd.end())
        {
            flags.push_back(*(at + sizeof(marker) + 2));
            at += sizeof(marker);
        }
        return flags;
    }

    std::vector<std::uint8_t> BlockData(const ffpsd::Layer& layer, const char (&key)[5])
    {
        const ffpsd::TaggedBlock* block = layer.GetTaggedBlockByKey(Fourcc(key));
        return block == nullptr ? std::vector<std::uint8_t>() : block->data;
    }
} // namespace

TEST(LayerBackgroundTest, PhotoshopsBackgroundIsRecognised)
{
    for (const std::string& path : {kRgbPsd, kGrayscalePsd, kRgbLevelsPsd})
    {
        const ffpsd::Document doc = ffpsd::Document::Parse(path);
        EXPECT_TRUE(doc.GetLayerByIndex(0)->IsBackground()) << path;
        EXPECT_FALSE(doc.GetLayerByIndex(1)->IsBackground()) << path;
    }
}

TEST(LayerBackgroundTest, ANewOneIsMarkedAsPhotoshopMarksIt)
{
    const std::vector<std::uint8_t> photoshop_file = ReadFile(kRgbPsd);
    const ffpsd::Document photoshop = ffpsd::Document::Parse(photoshop_file);
    const ffpsd::Layer* expected = photoshop.GetLayerByIndex(0);
    ffpsd::Document doc = NewDocument();

    const ffpsd::Layer* background = doc.AddBackgroundLayer("Background", Pattern(4, 3, 3));

    EXPECT_EQ(BlockData(*background, "lnsr"), BlockData(*expected, "lnsr"));
    EXPECT_EQ(BlockData(*background, "lspf"), BlockData(*expected, "lspf"));
    EXPECT_EQ(NormalLayerFlags(doc.Save()).at(0), NormalLayerFlags(photoshop_file).at(0));
}

TEST(LayerBackgroundTest, ItGoesUnderEverythingElse)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Layer* a = doc.AddLayer("a", Pattern(2, 2, 4));
    const ffpsd::Layer* b = doc.AddLayer("b");

    const ffpsd::Layer* background = doc.AddBackgroundLayer("Background", Pattern(4, 3, 3));

    ASSERT_EQ(doc.GetLayerCount(), 3u);
    EXPECT_EQ(doc.GetLayerByIndex(0), background);
    EXPECT_EQ(doc.GetLayerByIndex(1), a);
    EXPECT_EQ(doc.GetLayerByIndex(2), b);
    EXPECT_NE(LayerId(*background), 0u);
    EXPECT_FALSE(doc.GetHasRealMergedData());
}

TEST(LayerBackgroundTest, ItCoversTheCanvasWithoutTransparency)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);

    const ffpsd::Layer* background = doc.AddBackgroundLayer("Background", Pattern(4, 3, 1));

    const ffpsd::Rect bounds = background->GetBounds();
    EXPECT_EQ(bounds.top, 0);
    EXPECT_EQ(bounds.left, 0);
    EXPECT_EQ(bounds.GetWidth(), 4);
    EXPECT_EQ(bounds.GetHeight(), 3);
    EXPECT_EQ(background->GetPixels().channel_count, 1u);
    EXPECT_EQ(background->GetPixels().bytes, Pattern(4, 3, 1).bytes);
}

TEST(LayerBackgroundTest, TransparencyIsFlattenedOntoWhite)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    ffpsd::Image image = Pattern(4, 3, 2);
    std::fill(image.bytes.begin(), image.bytes.end(), std::uint8_t{0});
    image.bytes[3] = 200;
    std::fill(image.bytes.begin() + 12, image.bytes.end(), std::uint8_t{255});
    image.bytes[13] = 0;
    image.bytes[14] = 128;
    image.bytes[15] = 64;

    const ffpsd::Image pixels = doc.AddBackgroundLayer("Background", image)->GetPixels();

    // Opaque stays, clear is white, 0 at half alpha is 127, 200 at a quarter is 241.
    ASSERT_EQ(pixels.channel_count, 1u);
    const std::vector<std::uint8_t> expected = {0, 255, 127, 241, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(pixels.bytes, expected);
}

TEST(LayerBackgroundTest, SixteenBitWhiteIsFull)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 16);
    ffpsd::Image image = Pattern(4, 3, 2, 16);
    std::vector<std::uint16_t> samples(24, 0x1234);
    std::fill(samples.begin() + 12, samples.end(), std::uint16_t{0xFFFF});
    samples[12] = 0;
    std::memcpy(image.bytes.data(), samples.data(), image.bytes.size());

    const ffpsd::Image pixels = doc.AddBackgroundLayer("Background", image)->GetPixels();

    std::vector<std::uint16_t> read(12);
    std::memcpy(read.data(), pixels.bytes.data(), pixels.bytes.size());
    EXPECT_EQ(read[0], 0xFFFFu);
    EXPECT_EQ(read[1], 0x1234u);
}

TEST(LayerBackgroundTest, LabWhiteHasNeutralAAndB)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kLab);
    ffpsd::Image clear = Pattern(4, 3, 4);
    std::fill(clear.bytes.begin() + 3 * 12, clear.bytes.end(), std::uint8_t{0});

    const ffpsd::Image pixels = doc.AddBackgroundLayer("Background", clear)->GetPixels();

    std::vector<std::uint8_t> expected(12, 255);
    expected.insert(expected.end(), 24, 128);
    EXPECT_EQ(pixels.bytes, expected);
}

TEST(LayerBackgroundTest, OnlyAnImageOfTheCanvasSizeIsTaken)
{
    ffpsd::Document doc = NewDocument();

    EXPECT_THROW(doc.AddBackgroundLayer("five", Pattern(4, 3, 5)), std::invalid_argument);
    EXPECT_THROW(doc.AddBackgroundLayer("small", Pattern(3, 3, 3)), std::invalid_argument);
    EXPECT_THROW(doc.AddBackgroundLayer("deep", Pattern(4, 3, 3, 16)), std::invalid_argument);
    EXPECT_THROW(doc.AddBackgroundLayer("empty", ffpsd::Image()), std::invalid_argument);

    EXPECT_EQ(doc.GetLayerCount(), 0u);
}

TEST(LayerBackgroundTest, ADocumentHasOneAtMost)
{
    ffpsd::Document doc = NewDocument();
    doc.AddBackgroundLayer("Background", Pattern(4, 3, 3));

    EXPECT_THROW(doc.AddBackgroundLayer("second", Pattern(4, 3, 3)), std::logic_error);
    EXPECT_EQ(doc.GetLayerCount(), 1u);
}

TEST(LayerBackgroundTest, ItStaysAtTheBottom)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Layer* background = doc.AddBackgroundLayer("Background", Pattern(4, 3, 3));
    const ffpsd::Layer* a = doc.AddLayer("a");
    const ffpsd::Layer* b = doc.AddLayer("b");

    EXPECT_THROW(doc.MoveLayer(0, 2), std::invalid_argument);
    EXPECT_THROW(doc.MoveLayer(2, 0), std::invalid_argument);
    EXPECT_EQ(doc.GetLayerByIndex(0), background);
    EXPECT_EQ(doc.GetLayerByIndex(2), b);

    // Above it, layers move freely; without it, anything can be at the bottom.
    doc.MoveLayer(2, 1);
    EXPECT_EQ(doc.GetLayerByIndex(1), b);
    doc.RemoveLayer(0);
    doc.MoveLayer(1, 0);
    EXPECT_EQ(doc.GetLayerByIndex(0), a);
}

TEST(LayerBackgroundTest, ACopyOfItIsAnOrdinaryLayer)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Layer* photoshops_copy = doc.GetLayerByIndex(1);

    const ffpsd::Layer* copy = doc.AddLayer(*doc.GetLayerByIndex(0));

    EXPECT_FALSE(copy->IsBackground());
    EXPECT_TRUE(doc.GetLayerByIndex(0)->IsBackground());
    EXPECT_EQ(BlockData(*copy, "lnsr"), BlockData(*photoshops_copy, "lnsr"));
    EXPECT_EQ(BlockData(*copy, "lspf"), BlockData(*photoshops_copy, "lspf"));

    const std::vector<std::uint8_t> flags = NormalLayerFlags(doc.Save());
    ASSERT_EQ(flags.size(), 3u);
    EXPECT_EQ(flags[2], flags[1]);
}

TEST(LayerBackgroundTest, ItSurvivesSaving)
{
    ffpsd::Document doc = NewDocument();
    doc.AddLayer("top", Pattern(2, 2, 4));
    doc.AddBackgroundLayer("Background", Pattern(4, 3, 3));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    ASSERT_EQ(back.GetLayerCount(), 2u);
    EXPECT_TRUE(back.GetLayerByIndex(0)->IsBackground());
    EXPECT_FALSE(back.GetLayerByIndex(1)->IsBackground());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(4, 3, 3).bytes);
}

TEST(LayerBackgroundTest, UnsetMakesItAnOrdinaryLayerWhereItIs)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Layer* photoshops_copy = doc.GetLayerByIndex(1);
    const ffpsd::Layer* background = doc.GetLayerByIndex(0);
    const ffpsd::Image pixels = background->GetPixels();

    EXPECT_TRUE(doc.UnsetBackgroundLayer());

    EXPECT_EQ(doc.GetLayerByIndex(0), background);
    EXPECT_FALSE(background->IsBackground());
    EXPECT_EQ(BlockData(*background, "lnsr"), BlockData(*photoshops_copy, "lnsr"));
    EXPECT_EQ(BlockData(*background, "lspf"), BlockData(*photoshops_copy, "lspf"));
    EXPECT_EQ(background->GetPixels().bytes, pixels.bytes);

    const std::vector<std::uint8_t> flags = NormalLayerFlags(doc.Save());
    EXPECT_EQ(flags.at(0), flags.at(1));
}

TEST(LayerBackgroundTest, OnceUnsetItMovesLikeAnyOther)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Layer* former = doc.GetLayerByIndex(0);
    doc.UnsetBackgroundLayer();

    doc.MoveLayer(0, 1);

    EXPECT_EQ(doc.GetLayerByIndex(1), former);
    EXPECT_FALSE(doc.UnsetBackgroundLayer());
    EXPECT_FALSE(ffpsd::Document::Parse(doc.Save()).GetLayerByIndex(1)->IsBackground());
}

TEST(LayerBackgroundTest, SetMakesTheCanvasWhiteAroundTheLayer)
{
    ffpsd::Document doc = NewDocument();
    doc.AddLayer("a");
    ffpsd::Layer* small = doc.AddLayer("small", Pattern(2, 1, 3), 1, 1);

    doc.SetBackgroundLayer(1);

    EXPECT_EQ(doc.GetLayerByIndex(0), small);
    EXPECT_TRUE(small->IsBackground());
    EXPECT_EQ(small->GetBounds().left, 0);
    EXPECT_EQ(small->GetBounds().GetWidth(), 4);
    EXPECT_EQ(small->GetBounds().GetHeight(), 3);

    // Pattern(2, 1, 3) is 3, 10 | 17, 24 | 31, 38; it lands on pixels 5 and 6 of each plane.
    std::vector<std::uint8_t> expected;
    for (const std::uint8_t first : {std::uint8_t{3}, std::uint8_t{17}, std::uint8_t{31}})
    {
        std::vector<std::uint8_t> plane(12, 255);
        plane[5] = first;
        plane[6] = static_cast<std::uint8_t>(first + 7);
        expected.insert(expected.end(), plane.begin(), plane.end());
    }
    EXPECT_EQ(small->GetPixels().bytes, expected);
}

TEST(LayerBackgroundTest, SetFoldsOpacityInAndDropsTheBlendMode)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Image black = Pattern(4, 3, 3);
    std::fill(black.bytes.begin(), black.bytes.end(), std::uint8_t{0});
    ffpsd::Layer* layer = doc.AddLayer("black", black);
    layer->SetOpacity(51);
    layer->SetBlendKey(Fourcc("mul "));

    doc.SetBackgroundLayer(0);

    // Black at 20% over white: 255 * 204 / 255.
    EXPECT_EQ(layer->GetPixels().bytes, std::vector<std::uint8_t>(36, 204));
    EXPECT_EQ(layer->GetOpacity(), 255u);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("norm"));
}

TEST(LayerBackgroundTest, UnsetThenSetGivesPhotoshopsPixelsBack)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Image background = doc.GetLayerByIndex(0)->GetPixels();
    ffpsd::Image copy = doc.GetLayerByIndex(1)->GetPixels();
    copy.bytes.resize(copy.bytes.size() / 4 * 3); // its alpha is fully opaque

    doc.UnsetBackgroundLayer();
    doc.SetBackgroundLayer(0);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, background.bytes);

    doc.UnsetBackgroundLayer();
    doc.SetBackgroundLayer(1);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetName(), kBackgroundCopyName);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, copy.bytes);
}

TEST(LayerBackgroundTest, SetRefusesWhatCannotBeABackground)
{
    ffpsd::Document rgb = ffpsd::Document::Parse(kRgbPsd);
    EXPECT_THROW(rgb.SetBackgroundLayer(1), std::logic_error);
    EXPECT_THROW(rgb.SetBackgroundLayer(2), std::out_of_range);
    rgb.SetBackgroundLayer(0); // already it

    ffpsd::Document levels = ffpsd::Document::Parse(kRgbLevelsPsd);
    levels.UnsetBackgroundLayer();
    EXPECT_THROW(levels.SetBackgroundLayer(1), std::invalid_argument);

    ffpsd::Document group = NewDocument();
    SetSectionDivider(*group.AddLayer("end"), 3);
    EXPECT_THROW(group.SetBackgroundLayer(0), std::invalid_argument);
    EXPECT_FALSE(group.GetLayerByIndex(0)->IsBackground());
}

TEST(LayerBackgroundTest, AnEmptyLayerBecomesAWhiteBackground)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    const ffpsd::Layer* empty = doc.AddLayer("empty");

    doc.SetBackgroundLayer(0);

    EXPECT_TRUE(empty->IsBackground());
    EXPECT_EQ(empty->GetPixels().bytes, std::vector<std::uint8_t>(12, 255));
}
