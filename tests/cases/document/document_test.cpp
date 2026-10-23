#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
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

    // Owner is a Document or a Layer, which give their blocks through the same calls.
    template <class Owner> void ExpectTaggedBlocksAreARawDoor(Owner& owner)
    {
        const std::size_t count = owner.GetTaggedBlockCount();
        owner.SetTaggedBlock(Block("abcd", {1}));
        owner.SetTaggedBlock(Block("efgh", {2}));
        ASSERT_EQ(owner.GetTaggedBlockCount(), count + 2);
        const ffpsd::TaggedBlock* added = owner.GetTaggedBlockByIndex(count);
        EXPECT_EQ(added->key, Fourcc("abcd"));

        // The same key assigns in place, so the pointer stays and shows the new bytes.
        owner.SetTaggedBlock(Block("abcd", {7, 7}));
        EXPECT_EQ(owner.GetTaggedBlockCount(), count + 2);
        EXPECT_EQ(owner.GetTaggedBlockByKey(Fourcc("abcd")), added);
        EXPECT_EQ(added->data, (std::vector<std::uint8_t>{7, 7}));

        EXPECT_TRUE(owner.RemoveTaggedBlock(Fourcc("abcd")));
        EXPECT_FALSE(owner.RemoveTaggedBlock(Fourcc("abcd")));
        EXPECT_EQ(owner.GetTaggedBlockByKey(Fourcc("abcd")), nullptr);
        EXPECT_EQ(owner.GetTaggedBlockByIndex(count)->key, Fourcc("efgh"));
        EXPECT_THROW(owner.GetTaggedBlockByIndex(count + 1), std::out_of_range);
    }
} // namespace

TEST(DocumentTest, ANewOneIsAnEmptyPsdOfItsSizeAndMode)
{
    const ffpsd::Document doc = NewDocument();
    EXPECT_FALSE(doc.IsPsb());
    EXPECT_EQ(doc.GetLayerCount(), 0u);
    EXPECT_EQ(doc.GetImageResourceCount(), 0u);
    EXPECT_EQ(doc.GetTaggedBlockCount(), 0u);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());
    EXPECT_TRUE(doc.HasRealMergedData());
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 72.0);

    const std::pair<ffpsd::ColorMode, std::uint16_t> modes[] = {
        {ffpsd::ColorMode::kGrayscale, 1}, {ffpsd::ColorMode::kIndexed, 1}, {ffpsd::ColorMode::kDuotone, 1},
        {ffpsd::ColorMode::kRgb, 3},       {ffpsd::ColorMode::kLab, 3},     {ffpsd::ColorMode::kCmyk, 4},
    };
    for (const auto& [mode, channels] : modes)
    {
        const ffpsd::Document sized(40, 30, mode, 16);
        EXPECT_EQ(sized.GetWidth(), 40u);
        EXPECT_EQ(sized.GetHeight(), 30u);
        EXPECT_EQ(sized.GetColorMode(), mode);
        EXPECT_EQ(sized.GetChannelCount(), channels);
        EXPECT_EQ(sized.GetDepth(), 16u);
    }
    EXPECT_EQ(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb).GetDepth(), 8u);
    EXPECT_EQ(ffpsd::Document(4, 3, ffpsd::ColorMode::kBitmap, 1).GetChannelCount(), 1u);

    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kMultichannel), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(0, 3, ffpsd::ColorMode::kRgb), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb, 12), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kRgb, 1), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, ffpsd::ColorMode::kBitmap, 8), std::invalid_argument);
    EXPECT_THROW(ffpsd::Document(4, 3, static_cast<ffpsd::ColorMode>(5)), std::invalid_argument);

    // A side over 30000 makes it a PSB.
    EXPECT_FALSE(ffpsd::Document(30000, 3, ffpsd::ColorMode::kRgb).IsPsb());
    EXPECT_TRUE(ffpsd::Document(3, 30001, ffpsd::ColorMode::kRgb).IsPsb());
    EXPECT_THROW(ffpsd::Document(300001, 3, ffpsd::ColorMode::kRgb), std::invalid_argument);
}

TEST(DocumentTest, TheCompositeMatchesTheHeaderAndComesIntoTheCallersMemory)
{
    ffpsd::Document doc = NewDocument();
    doc.SetHasRealMergedData(false);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 2, 3)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 2, 8, ffpsd::ColorMode::kRgb)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 57)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 3, 16)), std::invalid_argument);
    EXPECT_THROW(doc.SetMergedImage(Pattern(4, 3, 3, 8, ffpsd::ColorMode::kLab)), std::invalid_argument);
    EXPECT_TRUE(doc.GetMergedImage().bytes.empty());

    // Without a composite there is nothing to give, and nothing is asked.
    EXPECT_TRUE(doc.GetMergedImageInfo().IsEmpty());
    EXPECT_NO_THROW(doc.GetMergedImageBytes(nullptr, 0));
    std::uint8_t one = 0;
    EXPECT_THROW(doc.GetMergedImageBytes(&one, 1), std::invalid_argument);

    doc.SetMergedImage(Pattern(4, 3, 3));
    EXPECT_EQ(doc.GetMergedImage().bytes, Pattern(4, 3, 3).bytes);
    EXPECT_EQ(doc.GetMergedImage().color_mode, ffpsd::ColorMode::kRgb);
    EXPECT_TRUE(doc.HasRealMergedData());

    // Channels past the colors are alpha channels, and the header counts them.
    doc.SetMergedImage(Pattern(4, 3, 5));
    EXPECT_EQ(doc.GetChannelCount(), 5u);
    EXPECT_EQ(RoundTrip(doc).GetMergedImage().bytes, Pattern(4, 3, 5).bytes);
    doc.SetMergedImage(Pattern(4, 3, 3));
    EXPECT_EQ(doc.GetChannelCount(), 3u);

    // Photoshop's RLE composite, then the same written raw.
    const ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    const ffpsd::Document raw = RoundTrip(photoshop, ffpsd::Compression::kRaw);
    for (const ffpsd::Document* stored : {&photoshop, &raw})
    {
        const ffpsd::Image image = stored->GetMergedImage();
        const ffpsd::ImageInfo info = stored->GetMergedImageInfo();
        EXPECT_EQ(info.width, image.width);
        EXPECT_EQ(info.height, image.height);
        EXPECT_EQ(info.channel_count, image.channel_count);
        EXPECT_EQ(info.depth, image.depth);
        EXPECT_EQ(info.GetSizeBytes(), image.bytes.size());

        // Filled with a pattern first, so a byte left unwritten would show.
        ffpsd::Bytes bytes(info.GetSizeBytes(), 0xAB);
        stored->GetMergedImageBytes(bytes.data(), bytes.size());
        EXPECT_EQ(bytes, image.bytes);
        EXPECT_THROW(stored->GetMergedImageBytes(bytes.data(), bytes.size() - 1), std::invalid_argument);
    }
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
    EXPECT_EQ(Pixels(moved, 0), WithOpaqueAlpha(Pattern(1, 1, 3)).bytes);
}

TEST(DocumentTest, TaggedBlocksAreARawDoorOnTheDocumentAndEveryLayer)
{
    ffpsd::Document doc = NewDocument();
    ExpectTaggedBlocksAreARawDoor(doc);
    ExpectTaggedBlocksAreARawDoor(*doc.AddLayer("blocks"));
}

TEST(DocumentTest, ResourcesAreARawDoorInIdOrder)
{
    ffpsd::Document doc = NewDocument();
    doc.SetImageResource(Resource(1000, {1}));
    doc.SetImageResource(Resource(3000, {3}));
    doc.SetImageResource(Resource(2000, {2}));
    ASSERT_EQ(doc.GetImageResourceCount(), 3u);
    EXPECT_EQ(doc.GetImageResourceByIndex(0)->id, 1000u);
    EXPECT_EQ(doc.GetImageResourceByIndex(1)->id, 2000u);
    EXPECT_EQ(doc.GetImageResourceByIndex(2)->id, 3000u);

    // An existing id assigns in place, so the pointer stays and shows the new bytes.
    const ffpsd::ImageResource* first = doc.GetImageResourceById(1000);
    doc.SetImageResource(Resource(1000, {9, 9}));
    EXPECT_EQ(doc.GetImageResourceCount(), 3u);
    EXPECT_EQ(doc.GetImageResourceById(1000), first);
    EXPECT_EQ(first->data, (std::vector<std::uint8_t>{9, 9}));

    const ffpsd::ImageResource* kept = doc.GetImageResourceById(2000);
    EXPECT_TRUE(doc.RemoveImageResource(1000));
    EXPECT_FALSE(doc.RemoveImageResource(1000));
    EXPECT_EQ(doc.GetImageResourceById(1000), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(2000), kept);
    EXPECT_THROW(doc.GetImageResourceByIndex(2), std::out_of_range);
}

TEST(DocumentTest, ResolutionAndVersionInfoAreCheckedAndKeptInTheirResources)
{
    ffpsd::Document doc = NewDocument();
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

    ffpsd::ResolutionInfo resolution;
    resolution.horizontal = 300.0;
    resolution.vertical = 150.5;
    resolution.horizontal_unit = 2;
    doc.SetResolutionInfo(resolution);
    EXPECT_NE(doc.GetImageResourceById(1005), nullptr);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().horizontal, 300.0);
    EXPECT_DOUBLE_EQ(doc.GetResolutionInfo().vertical, 150.5);
    EXPECT_EQ(doc.GetResolutionInfo().horizontal_unit, 2);

    ffpsd::VersionInfo version;
    version.has_real_merged_data = false;
    version.writer_name = "ffpsd";
    version.reader_name = kBackgroundName;
    version.file_version = 7;
    doc.SetVersionInfo(version);
    const ffpsd::VersionInfo back = doc.GetVersionInfo();
    EXPECT_FALSE(back.has_real_merged_data);
    EXPECT_EQ(back.writer_name, "ffpsd");
    EXPECT_EQ(back.reader_name, kBackgroundName);
    EXPECT_EQ(back.file_version, 7u);
    EXPECT_FALSE(doc.HasRealMergedData());

    // Setting only the flag keeps the names the writer left.
    ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    photoshop.SetHasRealMergedData(false);
    const ffpsd::VersionInfo flagged = photoshop.GetVersionInfo();
    EXPECT_FALSE(flagged.has_real_merged_data);
    EXPECT_EQ(flagged.writer_name, "Adobe Photoshop");
    EXPECT_EQ(flagged.reader_name, "Adobe Photoshop 2026");
}
