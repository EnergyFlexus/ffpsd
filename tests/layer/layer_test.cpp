#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(LayerTest, ANewLayerIsVisibleOpaqueAndNormalUntilSet)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("new");

    EXPECT_EQ(layer->GetKind(), ffpsd::LayerKind::kRaster);
    EXPECT_TRUE(layer->IsVisible());
    EXPECT_EQ(layer->GetOpacity(), 255u);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("norm"));
    EXPECT_EQ(layer->GetAdjustmentKey(), 0u);
    EXPECT_FALSE(layer->GetAdjustment<ffpsd::LevelsInfo>().has_value());

    // An unknown blend mode is kept as it is.
    layer->SetBlendKey(Fourcc("zzzz"));
    layer->SetOpacity(0);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("zzzz"));
    EXPECT_EQ(layer->GetOpacity(), 0u);

    layer->SetVisible(false);
    EXPECT_FALSE(layer->IsVisible());
    layer->SetVisible(false);
    EXPECT_FALSE(layer->IsVisible());
    layer->SetVisible(true);
    EXPECT_TRUE(layer->IsVisible());
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

TEST(LayerTest, TheKindComesFromTheSectionDivider)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("marker");

    const std::pair<std::uint32_t, ffpsd::LayerKind> kinds[] = {
        {1, ffpsd::LayerKind::kGroupOpen},
        {2, ffpsd::LayerKind::kGroupClosed},
        {3, ffpsd::LayerKind::kGroupEnd},
        {0, ffpsd::LayerKind::kRaster}};
    for (const auto& [type, kind] : kinds)
    {
        SetSectionDivider(*layer, type);
        EXPECT_EQ(layer->GetKind(), kind) << "type " << type;
    }

    // Too short to hold a type.
    layer->SetTaggedBlock(Block("lsct", {0, 1}));
    EXPECT_EQ(layer->GetKind(), ffpsd::LayerKind::kRaster);
}

TEST(LayerTest, TaggedBlocksAreARawDoor)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("blocks");
    const std::size_t count = layer->GetTaggedBlockCount();

    layer->SetTaggedBlock(Block("abcd", {1}));
    ASSERT_EQ(layer->GetTaggedBlockCount(), count + 1);
    const ffpsd::TaggedBlock* added = layer->GetTaggedBlockByIndex(count);
    EXPECT_EQ(added->key, Fourcc("abcd"));

    // The same key assigns in place, so the pointer stays and shows the new bytes.
    layer->SetTaggedBlock(Block("abcd", {2, 3}));
    EXPECT_EQ(layer->GetTaggedBlockCount(), count + 1);
    EXPECT_EQ(layer->GetTaggedBlockByKey(Fourcc("abcd")), added);
    EXPECT_EQ(added->data, (std::vector<std::uint8_t>{2, 3}));

    EXPECT_TRUE(layer->RemoveTaggedBlock(Fourcc("abcd")));
    EXPECT_FALSE(layer->RemoveTaggedBlock(Fourcc("abcd")));
    EXPECT_EQ(layer->GetTaggedBlockByKey(Fourcc("abcd")), nullptr);
    EXPECT_THROW(layer->GetTaggedBlockByIndex(count), std::out_of_range);
}
