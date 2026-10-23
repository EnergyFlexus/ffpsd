#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ffpsd_test;

TEST(LayerTest, ANewLayerIsVisibleOpaqueNormalAndUnclippedUntilSet)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("new");

    EXPECT_EQ(layer->GetKind(), ffpsd::LayerKind::kRaster);
    EXPECT_TRUE(layer->IsVisible());
    EXPECT_FALSE(layer->IsClipped());
    EXPECT_EQ(layer->GetOpacity(), 255u);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("norm"));
    EXPECT_EQ(layer->GetAdjustmentKey(), 0u);
    EXPECT_FALSE(layer->GetAdjustment<ffpsd::LevelsInfo>().has_value());

    // An unknown blend mode is kept as it is.
    layer->SetBlendKey(Fourcc("zzzz"));
    layer->SetOpacity(0);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("zzzz"));
    EXPECT_EQ(layer->GetOpacity(), 0u);

    for (const bool visible : {false, false, true})
    {
        layer->SetVisible(visible);
        EXPECT_EQ(layer->IsVisible(), visible);
    }
    for (const bool clipped : {true, false})
    {
        layer->SetClipped(clipped);
        EXPECT_EQ(layer->IsClipped(), clipped);
    }
}

TEST(LayerTest, TheNameIsUnicodeWithALegacyCopy)
{
    ffpsd::Document doc = NewDocument();

    // 'luni' holds the name as UTF-16; the record keeps its own legacy copy.
    ffpsd::Layer* unicode = doc.AddLayer(kColorFillName);
    ASSERT_NE(unicode->GetTaggedBlockByKey(Fourcc("luni")), nullptr);
    EXPECT_EQ(unicode->GetName(), kColorFillName);

    // A count of 1000 characters in a block of 6 bytes, then no block at all: the legacy name stands.
    ffpsd::Layer* plain = doc.AddLayer("plain");
    plain->SetTaggedBlock(Block("luni", {0, 0, 0x03, 0xE8, 0, 'x'}));
    EXPECT_EQ(plain->GetName(), "plain");
    plain->RemoveTaggedBlock(Fourcc("luni"));
    EXPECT_EQ(plain->GetName(), "plain");

    // A new name goes into both: without 'luni' the legacy copy has it too.
    plain->SetName(kBackgroundCopyName);
    EXPECT_EQ(plain->GetName(), kBackgroundCopyName);
    plain->RemoveTaggedBlock(Fourcc("luni"));
    EXPECT_EQ(plain->GetName(), kBackgroundCopyName);
}

TEST(LayerTest, TheKindComesFromTheSectionDividerAndOnlyAHeaderOpensAndCloses)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* marker = doc.AddLayer("marker");
    const std::pair<std::uint32_t, ffpsd::LayerKind> kinds[] = {
        {1, ffpsd::LayerKind::kGroupOpen},
        {2, ffpsd::LayerKind::kGroupClosed},
        {3, ffpsd::LayerKind::kGroupEnd},
        {0, ffpsd::LayerKind::kRaster},
    };
    for (const auto& [type, kind] : kinds)
    {
        SetSectionDivider(*marker, type);
        EXPECT_EQ(marker->GetKind(), kind) << "type " << type;
    }

    // Too short to hold a type.
    marker->SetTaggedBlock(Block("lsct", {0, 1}));
    EXPECT_EQ(marker->GetKind(), ffpsd::LayerKind::kRaster);

    // The type alone changes; the blend mode after it stays.
    ffpsd::Layer* group = doc.AddGroup("group");
    const std::vector<std::uint8_t> open = BlockData(*group, "lsct");
    std::vector<std::uint8_t> closed = open;
    closed[3] = 2;
    group->SetGroupOpen(false);
    EXPECT_EQ(group->GetKind(), ffpsd::LayerKind::kGroupClosed);
    EXPECT_EQ(BlockData(*group, "lsct"), closed);
    group->SetGroupOpen(true);
    EXPECT_EQ(group->GetKind(), ffpsd::LayerKind::kGroupOpen);
    EXPECT_EQ(BlockData(*group, "lsct"), open);

    // Neither the end marker nor a raster layer opens, and both stay as they were.
    ffpsd::Layer* end = doc.GetLayerByIndex(1);
    EXPECT_THROW(end->SetGroupOpen(true), std::logic_error);
    EXPECT_THROW(marker->SetGroupOpen(false), std::logic_error);
    EXPECT_EQ(end->GetKind(), ffpsd::LayerKind::kGroupEnd);
    EXPECT_EQ(marker->GetKind(), ffpsd::LayerKind::kRaster);
}
