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
} // namespace

TEST(DocumentResourcesTest, ResolutionRoundTripsAndCreatesItsResource)
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

TEST(DocumentResourcesTest, ResolutionOutsideSixteenDotSixteenIsRefusedWhole)
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

TEST(DocumentResourcesTest, TheCompositeFlagKeepsTheWriterNames)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    doc.SetHasRealMergedData(false);

    const ffpsd::VersionInfo info = doc.GetVersionInfo();
    EXPECT_FALSE(info.has_real_merged_data);
    EXPECT_EQ(info.writer_name, "Adobe Photoshop");
    EXPECT_EQ(info.reader_name, "Adobe Photoshop 2026");
}

TEST(DocumentResourcesTest, VersionInfoRoundTrips)
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

TEST(DocumentResourcesTest, ANewResourceIsInsertedInIdOrder)
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

TEST(DocumentResourcesTest, SettingAnExistingResourceAssignsInPlace)
{
    ffpsd::Document doc;
    doc.SetImageResource(Resource(1000, {1}));
    const ffpsd::ImageResource* before = doc.GetImageResourceById(1000);

    doc.SetImageResource(Resource(1000, {9, 9}));

    EXPECT_EQ(doc.GetImageResourceCount(), 1u);
    EXPECT_EQ(doc.GetImageResourceById(1000), before);
    EXPECT_EQ(before->data, (std::vector<std::uint8_t>{9, 9}));
}

TEST(DocumentResourcesTest, RemovingAResourceReportsWhetherItWasThere)
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
