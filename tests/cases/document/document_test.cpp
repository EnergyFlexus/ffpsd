#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentTest, ANewOneIsAnEmptyPsd)
{
    const ffpsd::Document doc = NewDocument();

    EXPECT_FALSE(doc.IsPsb());
    EXPECT_EQ(doc.GetLayerCount(), 0u);
    EXPECT_EQ(doc.GetImageResourceCount(), 0u);
    EXPECT_EQ(doc.GetTaggedBlockCount(), 0u);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());
    EXPECT_TRUE(doc.HasRealMergedData());
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 72.0);
}

TEST(DocumentTest, ANewOneOfASizeAndModeHasTheModesChannels)
{
    const std::pair<ffpsd::ColorMode, std::uint16_t> modes[] = {{ffpsd::ColorMode::kGrayscale, 1}, {ffpsd::ColorMode::kIndexed, 1},
                                                                {ffpsd::ColorMode::kDuotone, 1},   {ffpsd::ColorMode::kRgb, 3},
                                                                {ffpsd::ColorMode::kLab, 3},       {ffpsd::ColorMode::kCmyk, 4}};
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
    EXPECT_EQ(ffpsd::Document(4, 3, ffpsd::ColorMode::kBitmap, 1).GetChannelCount(), 1u);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kMultichannel), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(0, 3, ffpsd::ColorMode::kRgb), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb, 12), std::invalid_argument);

    EXPECT_FALSE(ffpsd::Document(30000, 3, ffpsd::ColorMode::kRgb).IsPsb());
    EXPECT_TRUE(ffpsd::Document(3, 30001, ffpsd::ColorMode::kRgb).IsPsb());
    EXPECT_THROW(ffpsd::Document(300001, 3, ffpsd::ColorMode::kRgb), std::invalid_argument);
}

TEST(DocumentTest, TheCompositeMatchesTheHeader)
{
    ffpsd::Document doc = NewDocument();
    doc.SetHasRealMergedData(false);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 2, 3)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 2, 8, ffpsd::ColorMode::kRgb)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 57)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 3, 16)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 3, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());

    doc.SetMergedImage(Pattern(4, 3, 3));
    EXPECT_EQ(doc.GetMergedImage().bytes, Pattern(4, 3, 3).bytes);
    EXPECT_EQ(doc.GetMergedImage().color_mode, ffpsd::ColorMode::kRgb);
    EXPECT_TRUE(doc.HasRealMergedData());

    // Channels past the colors are alpha channels, and the header counts them.
    doc.SetMergedImage(Pattern(4, 3, 5));
    EXPECT_EQ(doc.GetChannelCount(), 5u);
    EXPECT_EQ(ffpsd::Document::Parse(doc.Save()).GetMergedImage().bytes, Pattern(4, 3, 5).bytes);
    doc.SetMergedImage(Pattern(4, 3, 3));
    EXPECT_EQ(doc.GetChannelCount(), 3u);
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
    EXPECT_EQ(moved.GetLayerByIndex(0)->GetPixels().bytes, WithOpaqueAlpha(Pattern(1, 1, 3)).bytes);
}

TEST(DocumentTest, SectionBlocksAreARawDoor)
{
    ffpsd::Document doc = NewDocument();
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

TEST(DocumentTest, TheCompositeIntoTheCallersMemoryMatchesGetMergedImage)
{
    const ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    const ffpsd::Document raw = ffpsd::Document::Parse(photoshop.Save(ffpsd::Compression::kRaw));
    for (const ffpsd::Document* doc : {&photoshop, &raw})
    {
        const ffpsd::Image image = doc->GetMergedImage();
        const ffpsd::ImageInfo info = doc->GetMergedImageInfo();
        EXPECT_EQ(info.width, image.width);
        EXPECT_EQ(info.height, image.height);
        EXPECT_EQ(info.channel_count, image.channel_count);
        EXPECT_EQ(info.depth, image.depth);
        EXPECT_EQ(info.GetSizeBytes(), image.bytes.size());

        ffpsd::Bytes bytes(info.GetSizeBytes(), 0xAB);
        doc->GetMergedImageBytes(bytes.data(), bytes.size());
        EXPECT_EQ(bytes, image.bytes);
        EXPECT_THROW(doc->GetMergedImageBytes(bytes.data(), bytes.size() - 1), std::invalid_argument);
    }

    // Without a composite there is nothing to give, and nothing is asked.
    const ffpsd::Document empty = NewDocument();
    EXPECT_TRUE(empty.GetMergedImageInfo().IsEmpty());
    EXPECT_NO_THROW(empty.GetMergedImageBytes(nullptr, 0));
    std::uint8_t one = 0;
    EXPECT_THROW(empty.GetMergedImageBytes(&one, 1), std::invalid_argument);
}
