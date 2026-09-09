#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ffpsd_test;

namespace
{
    ffpsd::TaggedBlock Block(const char (&key)[5], std::vector<std::uint8_t> data)
    {
        ffpsd::TaggedBlock block;
        block.key = Fourcc(key);
        block.data = std::move(data);
        return block;
    }
} // namespace

TEST(LayerTest, ANewLayerIsVisibleOpaqueAndNormal)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Layer* layer = doc.AddLayer("new");

    EXPECT_EQ(layer->GetKind(), ffpsd::LayerKind::kRaster);
    EXPECT_TRUE(layer->IsVisible());
    EXPECT_EQ(layer->GetOpacity(), 255u);
    EXPECT_EQ(layer->GetBlendKey(), Fourcc("norm"));
    EXPECT_EQ(layer->GetAdjustmentKey(), 0u);
    EXPECT_FALSE(layer->GetLevels().has_value());
}

TEST(LayerTest, VisibilityTogglesBackAndForth)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::Layer* layer = doc.GetLayerByIndex(0);

    layer->SetVisible(false);
    EXPECT_FALSE(layer->IsVisible());
    layer->SetVisible(false);
    EXPECT_FALSE(layer->IsVisible());
    layer->SetVisible(true);
    EXPECT_TRUE(layer->IsVisible());
}

TEST(LayerTest, AnUnknownBlendModeIsKeptAsIs)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("new");

    layer->SetBlendKey(Fourcc("zzzz"));
    layer->SetOpacity(0);

    EXPECT_EQ(layer->GetBlendKey(), Fourcc("zzzz"));
    EXPECT_EQ(layer->GetOpacity(), 0u);
}

TEST(LayerTest, TheNameIsUnicodeAndWrittenTwice)
{
    ffpsd::Document doc = NewDocument();

    ffpsd::Layer* layer = doc.AddLayer(kColorFillName);

    // 'luni' holds the name as UTF-16; the record keeps its own legacy copy.
    ASSERT_NE(layer->GetTaggedBlockByKey(Fourcc("luni")), nullptr);
    EXPECT_EQ(layer->GetName(), kColorFillName);
}

TEST(LayerTest, TheNameFallsBackToTheLegacyOneWithoutAUsableLuni)
{
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("plain");

    // A count of 1000 characters in a block of 6 bytes.
    layer->SetTaggedBlock(Block("luni", {0, 0, 0x03, 0xE8, 0, 'x'}));
    EXPECT_EQ(layer->GetName(), "plain");

    layer->RemoveTaggedBlock(Fourcc("luni"));
    EXPECT_EQ(layer->GetName(), "plain");
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
