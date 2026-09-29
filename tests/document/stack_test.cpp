#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

using namespace ffpsd_test;

TEST(DocumentStackTest, AddLayerPutsItOnTopWithTheNextId)
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

TEST(DocumentStackTest, AStackChangeDropsWhatIndexesLayers)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ASSERT_NE(doc.GetImageResourceById(1024), nullptr);

    doc.RemoveLayer(1);

    EXPECT_EQ(doc.GetImageResourceById(1024), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(1026), nullptr);
    EXPECT_EQ(doc.GetImageResourceById(1072), nullptr);
    EXPECT_FALSE(doc.HasRealMergedData());
}

TEST(DocumentStackTest, MoveLayerKeepsEveryPointer)
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

TEST(DocumentStackTest, RemoveLayerKeepsTheOthersInOrder)
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

TEST(DocumentStackTest, ACopyGetsItsOwnIdAndLeavesTheSourceAlone)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Layer* source = doc.GetLayerByIndex(1);
    const std::uint32_t source_id = LayerId(*source);

    const ffpsd::Layer* copy = doc.AddLayerCopy(*source);

    EXPECT_EQ(copy->GetName(), source->GetName());
    EXPECT_EQ(copy->GetPixels().bytes, source->GetPixels().bytes);
    EXPECT_NE(LayerId(*copy), source_id);
    EXPECT_EQ(LayerId(*source), source_id);
}

TEST(DocumentStackTest, ACopyGoesAcrossDocumentsOfOneFormatOnly)
{
    const ffpsd::Document rgb = ffpsd::Document::Parse(kRgbPsd);
    const ffpsd::Document gray = ffpsd::Document::Parse(kGrayscalePsd);
    ffpsd::Document target = NewDocument();

    const ffpsd::Layer* copy = target.AddLayerCopy(*rgb.GetLayerByIndex(0));
    EXPECT_EQ(copy->GetName(), kBackgroundName);

    EXPECT_THROW(target.AddLayerCopy(*gray.GetLayerByIndex(0)), std::invalid_argument);
    EXPECT_EQ(target.GetLayerCount(), 1u);
}

TEST(DocumentStackTest, AStackChangeThatBreaksAGroupIsRolledBack)
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
    EXPECT_THROW(doc.AddLayerCopy(*header), std::invalid_argument);

    ASSERT_EQ(doc.GetLayerCount(), 3u);
    EXPECT_EQ(doc.GetLayerByIndex(0), end);
    EXPECT_EQ(doc.GetLayerByIndex(1), inside);
    EXPECT_EQ(doc.GetLayerByIndex(2), header);

    // Within the group, or out of it, a raster layer moves freely.
    doc.MoveLayer(1, 2);
    doc.MoveLayer(2, 1);
    EXPECT_EQ(doc.GetLayerByIndex(1), inside);
}

TEST(DocumentStackTest, EditsSurviveSaving)
{
    // The background stays at the bottom; the new layer goes under the copy.
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    doc.AddLayer("added", Pattern(2, 2, 4), 1, 2);
    doc.MoveLayer(2, 1);
    ffpsd::Layer* copy = doc.GetLayerByIndex(2);
    copy->SetOpacity(100);
    copy->SetVisible(false);
    copy->SetBlendKey(Fourcc("mul "));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    ASSERT_EQ(back.GetLayerCount(), 3u);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetName(), kBackgroundName);
    const ffpsd::Layer* copy_back = back.GetLayerByIndex(2);
    EXPECT_EQ(copy_back->GetName(), kBackgroundCopyName);
    EXPECT_EQ(copy_back->GetOpacity(), 100u);
    EXPECT_FALSE(copy_back->IsVisible());
    EXPECT_EQ(copy_back->GetBlendKey(), Fourcc("mul "));

    const ffpsd::Layer* added = back.GetLayerByIndex(1);
    EXPECT_EQ(added->GetName(), "added");
    EXPECT_EQ(added->GetBounds().top, 1);
    EXPECT_EQ(added->GetBounds().left, 2);
    EXPECT_EQ(added->GetPixels().bytes, Pattern(2, 2, 4).bytes);
    EXPECT_FALSE(back.HasRealMergedData());
}
