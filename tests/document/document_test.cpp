#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentTest, ANewOneIsAnEmptyEightBitRgbPsd)
{
    const ffpsd::Document doc;

    EXPECT_EQ(doc.GetWidth(), 0u);
    EXPECT_EQ(doc.GetHeight(), 0u);
    EXPECT_EQ(doc.GetChannelCount(), 0u);
    EXPECT_EQ(doc.GetDepth(), 8u);
    EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kRgb);
    EXPECT_FALSE(doc.IsPsb());
    EXPECT_EQ(doc.GetLayerCount(), 0u);
    EXPECT_EQ(doc.GetImageResourceCount(), 0u);
    EXPECT_EQ(doc.GetTaggedBlockCount(), 0u);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());
    EXPECT_TRUE(doc.HasRealMergedData());
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 72.0);
}

TEST(DocumentTest, SidesAreLimitedByTheFormat)
{
    ffpsd::Document doc;

    doc.SetWidth(30000);
    EXPECT_THROW(doc.SetWidth(30001), std::invalid_argument);
    EXPECT_THROW(doc.SetHeight(0), std::invalid_argument);
    EXPECT_EQ(doc.GetWidth(), 30000u);

    doc.SetPsb(true);
    doc.SetWidth(300000);
    doc.SetHeight(300000);
    EXPECT_THROW(doc.SetWidth(300001), std::invalid_argument);
    EXPECT_EQ(doc.GetWidth(), 300000u);
}

TEST(DocumentTest, ChannelCountAndDepthTakeOnlyWhatPsdHas)
{
    ffpsd::Document doc;

    doc.SetChannelCount(56);
    EXPECT_THROW(doc.SetChannelCount(57), std::invalid_argument);
    EXPECT_THROW(doc.SetChannelCount(0), std::invalid_argument);
    EXPECT_EQ(doc.GetChannelCount(), 56u);

    for (const std::uint16_t depth : {std::uint16_t{1}, std::uint16_t{8}, std::uint16_t{16}, std::uint16_t{32}})
    {
        doc.SetDepth(depth);
        EXPECT_EQ(doc.GetDepth(), depth);
    }
    EXPECT_THROW(doc.SetDepth(12), std::invalid_argument);
    EXPECT_EQ(doc.GetDepth(), 32u);
}

TEST(DocumentTest, ANewOneOfASizeAndModeHasTheModesChannels)
{
    const std::pair<ffpsd::ColorMode, std::uint16_t> modes[] = {
        {ffpsd::ColorMode::kGrayscale, 1}, {ffpsd::ColorMode::kRgb, 3}, {ffpsd::ColorMode::kLab, 3}, {ffpsd::ColorMode::kCmyk, 4}};
    for (const auto& [mode, channels] : modes)
    {
        const ffpsd::Document doc(40, 30, mode, 16);
        EXPECT_EQ(doc.GetWidth(), 40u);
        EXPECT_EQ(doc.GetHeight(), 30u);
        EXPECT_EQ(doc.GetColorMode(), mode);
        EXPECT_EQ(doc.GetChannelCount(), channels);
        EXPECT_EQ(doc.GetDepth(), 16u);
    }

    EXPECT_EQ(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb).GetDepth(), 8u);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kMultichannel), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(0, 3, ffpsd::ColorMode::kRgb), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb, 12), std::invalid_argument);
}

TEST(DocumentTest, OnceThereAreLayersChannelsDepthAndModeStay)
{
    ffpsd::Document doc = NewDocument();
    doc.AddLayer("a", Pattern(2, 2, 3));

    EXPECT_THROW(doc.SetDepth(16), std::logic_error);
    EXPECT_THROW(doc.SetChannelCount(4), std::logic_error);
    EXPECT_THROW(doc.SetColorMode(ffpsd::ColorMode::kGrayscale), std::logic_error);
    EXPECT_EQ(doc.GetDepth(), 8u);
    EXPECT_EQ(doc.GetChannelCount(), 3u);
    EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kRgb);
    EXPECT_EQ(doc.GetLayerByIndex(0)->GetPixels().bytes, Pattern(2, 2, 3).bytes);

    // The same value is no change, and a layer does not tie the canvas size: it may reach past it.
    doc.SetDepth(8);
    doc.SetColorMode(ffpsd::ColorMode::kRgb);
    doc.SetWidth(10);
    EXPECT_EQ(doc.GetWidth(), 10u);
}

TEST(DocumentTest, UnderABackgroundTheCanvasSizeStays)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);

    EXPECT_THROW(doc.SetWidth(100), std::logic_error);
    EXPECT_THROW(doc.SetHeight(100), std::logic_error);
    EXPECT_EQ(doc.GetWidth(), 1890u);
    EXPECT_EQ(doc.GetHeight(), 1417u);

    // Once it is an ordinary layer, nothing has to cover the canvas.
    doc.UnsetBackgroundLayer();
    doc.SetWidth(100);
    EXPECT_EQ(doc.GetWidth(), 100u);
}

TEST(DocumentTest, AHeaderChangeDropsTheComposite)
{
    ffpsd::Document doc = NewDocument();
    doc.SetMergedImage(Pattern(4, 3, 3));

    doc.SetWidth(4);
    EXPECT_EQ(doc.GetMergedImage().bytes, Pattern(4, 3, 3).bytes);

    doc.SetWidth(8);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());
    EXPECT_FALSE(doc.HasRealMergedData());

    // Saved, it is a blank composite of the new size.
    const ffpsd::Image merged = ffpsd::Document::Parse(doc.Save()).GetMergedImage();
    EXPECT_EQ(merged.width, 8u);
    EXPECT_EQ(merged.height, 3u);
    EXPECT_EQ(merged.channel_count, 3u);
}

TEST(DocumentTest, LayersFollowTheDocumentWhenItMoves)
{
    ffpsd::Document source = NewDocument();
    ffpsd::Layer* layer = source.AddLayer("a", Pattern(2, 2, 3));

    ffpsd::Document moved = std::move(source);
    moved.SetHasRealMergedData(true);
    layer->SetPixels(Pattern(1, 1, 3));

    // SetPixels reaches the document it now belongs to, not the emptied one.
    EXPECT_FALSE(moved.HasRealMergedData());
    EXPECT_EQ(moved.GetLayerByIndex(0)->GetPixels().bytes, Pattern(1, 1, 3).bytes);
}

TEST(DocumentTest, SectionBlocksAreARawDoor)
{
    ffpsd::Document doc;
    doc.SetTaggedBlock(Block("Patt", {1}));
    doc.SetTaggedBlock(Block("cinf", {2}));
    const ffpsd::TaggedBlock* patt = doc.GetTaggedBlockByKey(Fourcc("Patt"));

    doc.SetTaggedBlock(Block("Patt", {7, 7}));

    ASSERT_EQ(doc.GetTaggedBlockCount(), 2u);
    EXPECT_EQ(doc.GetTaggedBlockByIndex(0), patt);
    EXPECT_EQ(patt->data, (std::vector<std::uint8_t>{7, 7}));
    EXPECT_TRUE(doc.RemoveTaggedBlock(Fourcc("Patt")));
    EXPECT_FALSE(doc.RemoveTaggedBlock(Fourcc("Patt")));
    EXPECT_EQ(doc.GetTaggedBlockByIndex(0)->key, Fourcc("cinf"));
    EXPECT_THROW(doc.GetTaggedBlockByIndex(1), std::out_of_range);
}

TEST(DocumentTest, TheCompositeMustMatchTheDocument)
{
    ffpsd::Document doc = NewDocument();
    doc.SetHasRealMergedData(false);

    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 2, 3)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 4)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 3, 16)), std::invalid_argument);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());

    doc.SetMergedImage(Pattern(4, 3, 3));
    EXPECT_EQ(doc.GetMergedImage().bytes, Pattern(4, 3, 3).bytes);
    EXPECT_TRUE(doc.HasRealMergedData());
}
