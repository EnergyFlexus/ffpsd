#include "support/test_support.hpp"

#include <cmath>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ffpsd_test;

namespace
{
    ffpsd::ImageResource Resource(std::uint16_t id, std::vector<std::uint8_t> data)
    {
        ffpsd::ImageResource resource;
        resource.id = id;
        resource.data = std::move(data);
        return resource;
    }

    ffpsd::TaggedBlock Block(const char (&key)[5], std::vector<std::uint8_t> data)
    {
        ffpsd::TaggedBlock block;
        block.key = Fourcc(key);
        block.data = std::move(data);
        return block;
    }
} // namespace

// Header

TEST(DocumentTest, ANewOneIsAnEmptyEightBitRgbPsd)
{
    const ffpsd::Document doc;

    EXPECT_EQ(doc.GetWidth(), 0u);
    EXPECT_EQ(doc.GetHeight(), 0u);
    EXPECT_EQ(doc.GetChannelCount(), 0u);
    EXPECT_EQ(doc.GetDepth(), 8u);
    EXPECT_EQ(doc.GetColor(), ffpsd::ColorMode::kRgb);
    EXPECT_FALSE(doc.IsPsb());
    EXPECT_EQ(doc.GetLayerCount(), 0u);
    EXPECT_EQ(doc.GetImageResourceCount(), 0u);
    EXPECT_EQ(doc.GetTaggedBlockCount(), 0u);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());
    EXPECT_TRUE(doc.GetHasRealMergedData());
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

TEST(DocumentTest, ResolutionRoundTripsAndCreatesItsResource)
{
    ffpsd::Document doc;
    ffpsd::ResolutionInfo info;
    info.horizontal = 300.0;
    info.vertical = 150.5;
    info.horizontal_unit = 2;

    doc.SetResolutionInfo(info);

    EXPECT_NE(doc.GetImageResourceById(1005), nullptr);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 300.0);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().vertical, 150.5);
    EXPECT_EQ(doc.GetResolutionInfo().horizontal_unit, 2);
}

TEST(DocumentTest, ResolutionOutsideSixteenDotSixteenIsRefusedWhole)
{
    ffpsd::Document doc;
    for (const double bad : {0.0, -1.0, 32768.0, std::numeric_limits<double>::quiet_NaN()})
    {
        ffpsd::ResolutionInfo info;
        info.horizontal = 300.0;
        info.vertical = bad;
        EXPECT_THROW(doc.SetResolutionInfo(info), std::invalid_argument) << bad;
    }

    // The good half of a refused value is not stored either.
    EXPECT_EQ(doc.GetImageResourceById(1005), nullptr);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 72.0);
}

TEST(DocumentTest, TheCompositeFlagKeepsTheWriterNames)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    doc.SetHasRealMergedData(false);

    const ffpsd::VersionInfo info = doc.GetVersionInfo();
    EXPECT_FALSE(info.has_real_merged_data);
    EXPECT_EQ(info.writer_name, "Adobe Photoshop");
    EXPECT_EQ(info.reader_name, "Adobe Photoshop 2026");
}

TEST(DocumentTest, VersionInfoRoundTrips)
{
    ffpsd::Document doc;
    ffpsd::VersionInfo info;
    info.has_real_merged_data = false;
    info.writer_name = "ffpsd";
    info.reader_name = kBackgroundName;
    info.file_version = 7;

    doc.SetVersionInfo(info);

    const ffpsd::VersionInfo back = doc.GetVersionInfo();
    EXPECT_FALSE(back.has_real_merged_data);
    EXPECT_EQ(back.writer_name, "ffpsd");
    EXPECT_EQ(back.reader_name, kBackgroundName);
    EXPECT_EQ(back.file_version, 7u);
    EXPECT_FALSE(doc.GetHasRealMergedData());
}

// Image resources

TEST(DocumentTest, ANewResourceIsInsertedInIdOrder)
{
    ffpsd::Document doc;
    doc.SetImageResource(Resource(1000, {1}));
    doc.SetImageResource(Resource(3000, {3}));

    doc.SetImageResource(Resource(2000, {2}));

    ASSERT_EQ(doc.GetImageResourceCount(), 3u);
    EXPECT_EQ(doc.GetImageResourceByIndex(0)->id, 1000u);
    EXPECT_EQ(doc.GetImageResourceByIndex(1)->id, 2000u);
    EXPECT_EQ(doc.GetImageResourceByIndex(2)->id, 3000u);
}

TEST(DocumentTest, SettingAnExistingResourceAssignsInPlace)
{
    ffpsd::Document doc;
    doc.SetImageResource(Resource(1000, {1}));
    const ffpsd::ImageResource* before = doc.GetImageResourceById(1000);

    doc.SetImageResource(Resource(1000, {9, 9}));

    EXPECT_EQ(doc.GetImageResourceCount(), 1u);
    EXPECT_EQ(doc.GetImageResourceById(1000), before);
    EXPECT_EQ(before->data, (std::vector<std::uint8_t>{9, 9}));
}

TEST(DocumentTest, RemovingAResourceReportsWhetherItWasThere)
{
    ffpsd::Document doc;
    doc.SetImageResource(Resource(1000, {1}));
    doc.SetImageResource(Resource(2000, {2}));
    const ffpsd::ImageResource* kept = doc.GetImageResourceById(2000);

    EXPECT_TRUE(doc.RemoveImageResource(1000));
    EXPECT_FALSE(doc.RemoveImageResource(1000));
    EXPECT_EQ(doc.GetImageResourceById(1000), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(2000), kept);
    EXPECT_THROW(doc.GetImageResourceByIndex(1), std::out_of_range);
}

// Layer stack

TEST(DocumentTest, AddLayerPutsItOnTopWithTheNextId)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const std::uint32_t seed = BigEndianU32(doc.GetImageResourceById(1044)->data);

    const ffpsd::Layer* added = doc.AddLayer("top");

    ASSERT_EQ(doc.GetLayerCount(), 3u);
    EXPECT_EQ(doc.GetLayerByIndex(2), added);
    EXPECT_EQ(added->GetName(), "top");
    EXPECT_EQ(LayerId(*added), seed + 1);
    EXPECT_EQ(BigEndianU32(doc.GetImageResourceById(1044)->data), seed + 1);
}

TEST(DocumentTest, AStackChangeDropsWhatIndexesLayers)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ASSERT_NE(doc.GetImageResourceById(1024), nullptr);

    doc.RemoveLayer(1);

    EXPECT_EQ(doc.GetImageResourceById(1024), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(1026), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(1072), nullptr);
    EXPECT_FALSE(doc.GetHasRealMergedData());
}

TEST(DocumentTest, MoveLayerKeepsEveryPointer)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* a = doc.AddLayer("a");
    ffpsd::Layer* b = doc.AddLayer("b");
    ffpsd::Layer* c = doc.AddLayer("c");

    doc.MoveLayer(0, 2);
    EXPECT_EQ(doc.GetLayerByIndex(0), b);
    EXPECT_EQ(doc.GetLayerByIndex(1), c);
    EXPECT_EQ(doc.GetLayerByIndex(2), a);

    doc.MoveLayer(2, 0);
    EXPECT_EQ(doc.GetLayerByIndex(0), a);
    EXPECT_EQ(doc.GetLayerByIndex(2), c);

    EXPECT_THROW(doc.MoveLayer(0, 3), std::out_of_range);
    EXPECT_THROW(doc.MoveLayer(3, 0), std::out_of_range);
}

TEST(DocumentTest, RemoveLayerKeepsTheOthersInOrder)
{
    ffpsd::Document doc = NewDocument();
    const ffpsd::Layer* a = doc.AddLayer("a");
    doc.AddLayer("b");
    const ffpsd::Layer* c = doc.AddLayer("c");

    doc.RemoveLayer(1);

    ASSERT_EQ(doc.GetLayerCount(), 2u);
    EXPECT_EQ(doc.GetLayerByIndex(0), a);
    EXPECT_EQ(doc.GetLayerByIndex(1), c);
    EXPECT_THROW(doc.RemoveLayer(2), std::out_of_range);
    EXPECT_THROW(doc.GetLayerByIndex(2), std::out_of_range);
}

TEST(DocumentTest, ACopyGetsItsOwnIdAndLeavesTheSourceAlone)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Layer* source = doc.GetLayerByIndex(1);
    const std::uint32_t source_id = LayerId(*source);

    const ffpsd::Layer* copy = doc.AddLayer(*source);

    EXPECT_EQ(copy->GetName(), source->GetName());
    EXPECT_EQ(copy->GetPixels().bytes, source->GetPixels().bytes);
    EXPECT_NE(LayerId(*copy), source_id);
    EXPECT_EQ(LayerId(*source), source_id);
}

TEST(DocumentTest, ACopyGoesAcrossDocumentsOfOneFormatOnly)
{
    const ffpsd::Document rgb = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Document gray = ffpsd::Document::Parse(kGrayscalePsd);
    ffpsd::Document target = NewDocument();

    const ffpsd::Layer* copy = target.AddLayer(*rgb.GetLayerByIndex(0));
    EXPECT_EQ(copy->GetName(), kBackgroundName);

    EXPECT_THROW(target.AddLayer(*gray.GetLayerByIndex(0)), std::invalid_argument);
    EXPECT_EQ(target.GetLayerCount(), 1u);
}

TEST(DocumentTest, AStackChangeThatBreaksAGroupIsRolledBack)
{
    // Bottom to top: the end marker, a layer inside, the group header.
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* end = doc.AddLayer("end");
    ffpsd::Layer* inside = doc.AddLayer("inside");
    ffpsd::Layer* header = doc.AddLayer("group");
    SetSectionDivider(*end, 3);
    SetSectionDivider(*header, 1);
    ASSERT_EQ(header->GetKind(), ffpsd::LayerKind::kGroupOpen);

    EXPECT_THROW(doc.MoveLayer(2, 0), std::invalid_argument);
    EXPECT_THROW(doc.RemoveLayer(0), std::invalid_argument);
    EXPECT_THROW(doc.AddLayer(*header), std::invalid_argument);

    ASSERT_EQ(doc.GetLayerCount(), 3u);
    EXPECT_EQ(doc.GetLayerByIndex(0), end);
    EXPECT_EQ(doc.GetLayerByIndex(1), inside);
    EXPECT_EQ(doc.GetLayerByIndex(2), header);

    // Within the group, or out of it, a raster layer moves freely.
    doc.MoveLayer(1, 2);
    doc.MoveLayer(2, 1);
    EXPECT_EQ(doc.GetLayerByIndex(1), inside);
}

TEST(DocumentTest, LayersFollowTheDocumentWhenItMoves)
{
    ffpsd::Document source = NewDocument();
    ffpsd::Layer* layer = source.AddLayer("a", Pattern(2, 2, 3));

    ffpsd::Document moved = std::move(source);
    moved.SetHasRealMergedData(true);
    layer->SetPixels(Pattern(1, 1, 3));

    // SetPixels reaches the document it now belongs to, not the emptied one.
    EXPECT_FALSE(moved.GetHasRealMergedData());
    EXPECT_EQ(moved.GetLayerByIndex(0)->GetPixels().bytes, Pattern(1, 1, 3).bytes);
}

// Section 4 blocks

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

// Composite

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
    EXPECT_TRUE(doc.GetHasRealMergedData());
}

TEST(DocumentTest, RenderingTheCompositeIsNotThereYet)
{
    const ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    EXPECT_THROW(doc.RenderMergedImage(), std::logic_error);
}

// Parsing errors

TEST(DocumentTest, WhatIsNotAPsdIsRefused)
{
    const std::vector<std::uint8_t> empty;
    const std::vector<std::uint8_t> other = {'8', 'B', 'P', 'X', 0, 1, 0, 0, 0, 0, 0, 0, 0, 3};
    std::vector<std::uint8_t> cut = ReadFile(kRgbPsd);
    cut.resize(cut.size() / 3);

    EXPECT_THROW(ffpsd::Document::Parse(empty), std::runtime_error);
    EXPECT_THROW(ffpsd::Document::Parse(other), std::runtime_error);
    EXPECT_THROW(ffpsd::Document::Parse(cut), std::runtime_error);
}

TEST(DocumentTest, AMissingFileIsAFilesystemError)
{
    const std::filesystem::path missing = std::filesystem::path(FFPSD_TEST_DATA_DIR) / "no_such_file.psd";

    EXPECT_THROW(ffpsd::Document::Parse(missing.string()), std::filesystem::filesystem_error);
}

TEST(DocumentTest, BytesAndPathGiveTheSameDocument)
{
    const std::vector<std::uint8_t> bytes = ReadFile(kGrayscalePsd);

    const ffpsd::Document from_path = ffpsd::Document::Parse(kGrayscalePsd);
    const ffpsd::Document from_pointer = ffpsd::Document::Parse(bytes.data(), bytes.size());

    EXPECT_EQ(from_pointer.Save(), from_path.Save());
}
