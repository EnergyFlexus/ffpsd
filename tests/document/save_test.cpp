#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <system_error>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentSaveTest, PhotoshopFilesComeBackByteForByte)
{
    for (const std::string& path : {kGrayscalePsd, kRgbPsd, kRgbLevelsPsd, kGrayscaleLevelsPsd})
    {
        const std::vector<std::uint8_t> original = ReadFile(path);
        ASSERT_FALSE(original.empty()) << path;

        const std::vector<std::uint8_t> saved = ffpsd::Document::Parse(original).Save();

        ASSERT_EQ(saved.size(), original.size()) << path;
        EXPECT_TRUE(saved == original) << path;
    }
}

TEST(DocumentSaveTest, ResourcesAndBlocksSurviveWithPhotoshopsPadding)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);
    ffpsd::ImageResource resource;
    resource.id = 4000;
    resource.name = "odd";
    resource.data = {1, 2, 3};
    doc.SetImageResource(resource);
    doc.GetLayerByIndex(0)->SetTaggedBlock(Block("abcd", {4, 5, 6, 7, 8}));
    doc.AddLayer("Layer 1", Pattern(2, 2, 4));

    const std::vector<std::uint8_t> saved = doc.Save();
    const ffpsd::Document back = ffpsd::Document::Parse(saved);

    const ffpsd::ImageResource* read = back.GetImageResourceById(4000);
    ASSERT_NE(read, nullptr);
    EXPECT_EQ(read->name, "odd");
    EXPECT_EQ(read->data, resource.data);

    // A layer block comes back padded to 4, the padding now part of its data.
    const ffpsd::TaggedBlock* layer_block = back.GetLayerByIndex(0)->GetTaggedBlockByKey(Fourcc("abcd"));
    ASSERT_NE(layer_block, nullptr);
    EXPECT_EQ(layer_block->data, (std::vector<std::uint8_t>{4, 5, 6, 7, 8, 0, 0, 0}));

    // 'luni' of "Layer 1" is 18 bytes; Photoshop calls the file incompatible unless the padding is in the length.
    const std::uint8_t luni[] = {'8', 'B', 'I', 'M', 'l', 'u', 'n', 'i', 0, 0, 0, 20};
    const auto at = std::search(saved.begin(), saved.end(), std::begin(luni), std::end(luni));
    ASSERT_NE(at, saved.end());
    EXPECT_EQ(std::string(at + 12 + 20, at + 12 + 24), "8BIM");
}

TEST(DocumentSaveTest, TheCompositeStaysOrIsWrittenBlank)
{
    ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    photoshop.RemoveLayer(1);
    photoshop.RemoveLayer(0);
    const ffpsd::Document without_layers = ffpsd::Document::Parse(photoshop.Save());
    EXPECT_EQ(without_layers.GetLayerCount(), 0u);
    EXPECT_EQ(PlaneSum(without_layers.GetMergedImage(), 0), 183326186u);

    // A document made from scratch has no composite of its own, in either compression.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    doc.AddLayer("one", Pattern(4, 3, 2));
    for (const ffpsd::Compression compression : {ffpsd::Compression::kRaw, ffpsd::Compression::kRle})
    {
        const ffpsd::Document back = ffpsd::Document::Parse(doc.Save(compression));
        EXPECT_EQ(back.GetColorMode(), ffpsd::ColorMode::kGrayscale);
        EXPECT_EQ(back.GetWidth(), 4u);
        EXPECT_EQ(back.GetHeight(), 3u);
        ASSERT_EQ(back.GetLayerCount(), 1u);
        EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(4, 3, 2).bytes);
        EXPECT_EQ(back.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3, 0));
    }
}

TEST(DocumentSaveTest, DeepLayersGoIntoTheirOwnBlock)
{
    // 16 and 32 bit layers go into 'Lr16' and 'Lr32'; the samples stay in native order.
    for (const std::uint16_t depth : {std::uint16_t{16}, std::uint16_t{32}})
    {
        ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, depth);
        doc.AddLayer("deep", Pattern(3, 2, 4, depth));
        doc.SetMergedImage(Pattern(4, 3, 3, depth));

        const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

        EXPECT_NE(back.GetTaggedBlockByKey(depth == 16 ? Fourcc("Lr16") : Fourcc("Lr32")), nullptr);
        ASSERT_EQ(back.GetLayerCount(), 1u);
        EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(3, 2, 4, depth).bytes);
        EXPECT_EQ(back.GetMergedImage().bytes, Pattern(4, 3, 3, depth).bytes);
    }
}

TEST(DocumentSaveTest, APathGetsTheSameBytesOrASystemError)
{
    const std::filesystem::path path = std::filesystem::path(testing::TempDir()) / "ffpsd_save_test.psd";
    const std::filesystem::path unwritable = std::filesystem::path(testing::TempDir()) / "no_such_directory" / "out.psd";
    const ffpsd::Document doc = ffpsd::Document::Open(kGrayscalePsd);

    doc.Save(path.string());

    EXPECT_EQ(ReadFile(path.string()), ReadFile(kGrayscalePsd));
    EXPECT_THROW(doc.Save(unwritable.string()), std::system_error);
    std::filesystem::remove(path);

    // A path is UTF-8 whatever the system's code page; "test" in Cyrillic, written as bytes for any compiler.
    const std::filesystem::path cyrillic =
        std::filesystem::path(testing::TempDir()) / std::filesystem::u8path("ffpsd_\xD1\x82\xD0\xB5\xD1\x81\xD1\x82.psd");
    doc.Save(cyrillic.u8string());
    EXPECT_TRUE(std::filesystem::exists(cyrillic));
    EXPECT_EQ(ffpsd::Document::Open(cyrillic.u8string()).Save(), doc.Save());
    std::filesystem::remove(cyrillic);
}
