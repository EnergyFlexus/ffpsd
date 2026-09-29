#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace ffpsd_test;

namespace
{
    struct PsdFile
    {
        const char* name;
        std::string path;
    };

    std::string FileName(const testing::TestParamInfo<PsdFile>& info)
    {
        return info.param.name;
    }

    std::filesystem::path TempPath(const char* name)
    {
        return std::filesystem::path(testing::TempDir()) / name;
    }
} // namespace

class DocumentSaveUnchangedTest : public testing::TestWithParam<PsdFile>
{
};

TEST_P(DocumentSaveUnchangedTest, GivesBackTheSameBytes)
{
    const std::vector<std::uint8_t> original = ReadFile(GetParam().path);
    ASSERT_FALSE(original.empty());

    const std::vector<std::uint8_t> saved = ffpsd::Document::Parse(original).Save();

    ASSERT_EQ(saved.size(), original.size());
    EXPECT_TRUE(saved == original);
}

INSTANTIATE_TEST_SUITE_P(
    PhotoshopFiles, DocumentSaveUnchangedTest,
    testing::Values(
        PsdFile{"grayscale_two_layers", kGrayscalePsd}, PsdFile{"rgb_two_layers", kRgbPsd}, PsdFile{"rgb_levels", kRgbLevelsPsd}),
    FileName);

TEST(DocumentSaveTest, LayerBlocksCountTheirPaddingAsPhotoshopDoes)
{
    // 'luni' of "Layer 1" is 18 bytes; Photoshop calls the file incompatible unless the padding is in the length.
    ffpsd::Document doc = NewDocument();
    ffpsd::Layer* layer = doc.AddLayer("Layer 1", Pattern(2, 2, 4));
    ffpsd::TaggedBlock odd;
    odd.key = Fourcc("abcd");
    odd.data = {1, 2, 3, 4, 5};
    layer->SetTaggedBlock(odd);

    const std::vector<std::uint8_t> saved = doc.Save();

    const std::uint8_t luni[] = {'8', 'B', 'I', 'M', 'l', 'u', 'n', 'i'};
    const auto at = std::search(saved.begin(), saved.end(), std::begin(luni), std::end(luni));
    ASSERT_NE(at, saved.end());
    const std::uint32_t length = BigEndianU32(std::vector<std::uint8_t>(at + 8, at + 12));
    EXPECT_EQ(length, 20u);
    EXPECT_EQ(std::string(at + 12 + length, at + 12 + length + 4), "8BIM");

    const ffpsd::Document back = ffpsd::Document::Parse(saved);
    const ffpsd::TaggedBlock* read = back.GetLayerByIndex(0)->GetTaggedBlockByKey(Fourcc("abcd"));
    ASSERT_NE(read, nullptr);
    EXPECT_EQ(read->data, (std::vector<std::uint8_t>{1, 2, 3, 4, 5, 0, 0, 0}));
}

TEST(DocumentSaveTest, ResourcesAndBlocksSurvive)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::ImageResource resource;
    resource.id = 4000;
    resource.name = "odd";
    resource.data = {1, 2, 3};
    doc.SetImageResource(resource);
    ffpsd::TaggedBlock block;
    block.key = Fourcc("abcd");
    block.data = {4, 5, 6, 7, 8};
    doc.GetLayerByIndex(0)->SetTaggedBlock(block);

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    const ffpsd::ImageResource* read = back.GetImageResourceById(4000);
    ASSERT_NE(read, nullptr);
    EXPECT_EQ(read->name, "odd");
    EXPECT_EQ(read->data, resource.data);
    const ffpsd::TaggedBlock* layer_block = back.GetLayerByIndex(0)->GetTaggedBlockByKey(Fourcc("abcd"));
    ASSERT_NE(layer_block, nullptr);
    // A layer block comes back padded to 4, the padding now part of its data.
    std::vector<std::uint8_t> padded = block.data;
    padded.resize(8, 0);
    EXPECT_EQ(layer_block->data, padded);
}

TEST(DocumentSaveTest, WithoutLayersTheCompositeStays)
{
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);
    doc.RemoveLayer(1);
    doc.RemoveLayer(0);

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerCount(), 0u);
    EXPECT_EQ(PlaneSum(back.GetMergedImage(), 0), 678522862u);
}

TEST(DocumentSaveTest, ADocumentFromScratchGetsABlankComposite)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    doc.AddLayer("one", Pattern(4, 3, 2));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetColor(), ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(back.GetWidth(), 4u);
    EXPECT_EQ(back.GetHeight(), 3u);
    ASSERT_EQ(back.GetLayerCount(), 1u);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(4, 3, 2).bytes);
    EXPECT_EQ(back.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3, 0));
}

class DocumentSaveDeepTest : public testing::TestWithParam<std::uint16_t>
{
};

// 16 and 32 bit layers go into 'Lr16' and 'Lr32'; the samples stay in native order.
TEST_P(DocumentSaveDeepTest, LayersGoIntoTheirOwnBlock)
{
    const std::uint16_t depth = GetParam();
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, depth);
    doc.AddLayer("deep", Pattern(3, 2, 4, depth));
    doc.SetMergedImage(Pattern(4, 3, 3, depth));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_NE(back.GetTaggedBlockByKey(depth == 16 ? Fourcc("Lr16") : Fourcc("Lr32")), nullptr);
    ASSERT_EQ(back.GetLayerCount(), 1u);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(3, 2, 4, depth).bytes);
    EXPECT_EQ(back.GetMergedImage().bytes, Pattern(4, 3, 3, depth).bytes);
}

INSTANTIATE_TEST_SUITE_P(Depths, DocumentSaveDeepTest, testing::Values<std::uint16_t>(16, 32));

TEST(DocumentSaveTest, ADocumentWithoutASizeIsRefused)
{
    EXPECT_THROW(ffpsd::Document().Save(), std::logic_error);
}

TEST(DocumentSaveTest, APathGetsTheSameBytes)
{
    const std::filesystem::path path = TempPath("ffpsd_save_test.psd");

    ffpsd::Document::Parse(kGrayscalePsd).Save(path.string());

    EXPECT_EQ(ReadFile(path.string()), ReadFile(kGrayscalePsd));
    std::filesystem::remove(path);
}

TEST(DocumentSaveTest, AnUnwritablePathIsASystemError)
{
    const std::filesystem::path path = TempPath("no_such_directory") / "out.psd";
    const ffpsd::Document doc = ffpsd::Document::Parse(kGrayscalePsd);

    EXPECT_THROW(doc.Save(path.string()), std::system_error);
}
