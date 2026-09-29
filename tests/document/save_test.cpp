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
        PsdFile{"grayscale_two_layers", kGrayscalePsd}, PsdFile{"rgb_two_layers", kRgbPsd},
        PsdFile{"rgb_levels", kRgbLevelsPsd}),
    FileName);

TEST(DocumentSaveTest, StackEditsSurvive)
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
    EXPECT_FALSE(back.GetHasRealMergedData());
}

TEST(DocumentSaveTest, LayerBlocksCountTheirPaddingAsPhotoshopDoes)
{
    // "Layer 1" is 18 bytes of 'luni'. Photoshop reads a layer's blocks by their length alone, so the
    // padding to 4 has to be in it; otherwise it calls the file incompatible.
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

TEST(DocumentSaveTest, APsbRoundTrips)
{
    ffpsd::Document doc = NewDocument();
    doc.SetPsb(true);
    doc.AddLayer("one", Pattern(3, 2, 4));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_TRUE(back.IsPsb());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    EXPECT_EQ(back.GetMergedImage().bytes.size(), 4u * 3 * 3);
}

TEST(DocumentSaveTest, SwitchingTheFormatRepacksTheRows)
{
    // Photoshop's RLE, with 2 byte row counts, through a PSB with 4 byte ones and back.
    const ffpsd::Document original = ffpsd::Document::Parse(kRgbPsd);
    ffpsd::Document doc = ffpsd::Document::Parse(kRgbPsd);

    doc.SetPsb(true);
    const ffpsd::Document psb = ffpsd::Document::Parse(doc.Save());
    doc.SetPsb(false);
    const ffpsd::Document psd = ffpsd::Document::Parse(doc.Save());

    EXPECT_TRUE(psb.IsPsb());
    EXPECT_FALSE(psd.IsPsb());
    for (const ffpsd::Document* back : {&psb, &psd})
    {
        EXPECT_EQ(back->GetMergedImage().bytes, original.GetMergedImage().bytes);
        for (std::size_t i = 0; i < 2; ++i)
            EXPECT_EQ(back->GetLayerByIndex(i)->GetPixels().bytes, original.GetLayerByIndex(i)->GetPixels().bytes);
    }
}

TEST(DocumentSaveTest, RawAsksForUnpackedPixels)
{
    const ffpsd::Document original = ffpsd::Document::Parse(kRgbPsd);

    const std::vector<std::uint8_t> raw = original.Save(ffpsd::Compression::kRaw);

    // Section 5 ends the file: a compression field of 0, then every plane of 1890 x 1417 x 3.
    const std::size_t composite = std::size_t{1890} * 1417 * 3;
    ASSERT_GT(raw.size(), composite + 2);
    EXPECT_EQ(raw[raw.size() - composite - 2], 0u);
    EXPECT_EQ(raw[raw.size() - composite - 1], 0u);

    const ffpsd::Document back = ffpsd::Document::Parse(raw);
    EXPECT_EQ(back.GetMergedImage().bytes, original.GetMergedImage().bytes);
    for (std::size_t i = 0; i < 2; ++i)
        EXPECT_EQ(back.GetLayerByIndex(i)->GetPixels().bytes, original.GetLayerByIndex(i)->GetPixels().bytes);
}

TEST(DocumentSaveTest, RawAndRleGoBothWays)
{
    const ffpsd::Document original = ffpsd::Document::Parse(kRgbPsd);
    const std::vector<std::uint8_t> raw = original.Save(ffpsd::Compression::kRaw);

    const std::vector<std::uint8_t> rle = ffpsd::Document::Parse(raw).Save();

    EXPECT_LT(rle.size(), raw.size());
    const ffpsd::Document back = ffpsd::Document::Parse(rle);
    EXPECT_EQ(back.GetMergedImage().bytes, original.GetMergedImage().bytes);
    EXPECT_EQ(back.GetLayerByIndex(1)->GetPixels().bytes, original.GetLayerByIndex(1)->GetPixels().bytes);
}

TEST(DocumentSaveTest, ABlankCompositeComesInEitherCompression)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Document raw = ffpsd::Document::Parse(doc.Save(ffpsd::Compression::kRaw));
    const ffpsd::Document rle = ffpsd::Document::Parse(doc.Save(ffpsd::Compression::kRle));

    EXPECT_EQ(raw.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3 * 3, 0));
    EXPECT_EQ(rle.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3 * 3, 0));
}

TEST(DocumentSaveTest, FlatPixelsAreWrittenSmall)
{
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 1000, 1000);
    ffpsd::Image flat = Pattern(1000, 1000, 4);
    std::fill(flat.bytes.begin(), flat.bytes.end(), std::uint8_t{200});
    doc.AddLayer("flat", flat);
    ffpsd::Image merged = Pattern(1000, 1000, 3);
    std::fill(merged.bytes.begin(), merged.bytes.end(), std::uint8_t{200});
    doc.SetMergedImage(merged);

    const std::vector<std::uint8_t> saved = doc.Save();

    // Raw 7 MB; a flat row of 1000 bytes is 8 runs of 2 bytes plus its 2 byte count: 7000 x 18.
    EXPECT_LT(saved.size(), 126000u + 1000u);
    const ffpsd::Document back = ffpsd::Document::Parse(saved);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, flat.bytes);
    EXPECT_EQ(back.GetMergedImage().bytes, merged.bytes);
}

TEST(DocumentSaveTest, PixelsThatDoNotPackAreWrittenRaw)
{
    // Every byte differs from its neighbour, so RLE would only grow them.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 64);
    ffpsd::Image noise = Pattern(64, 64, 4);
    for (std::size_t i = 0; i < noise.bytes.size(); ++i)
        noise.bytes[i] = static_cast<std::uint8_t>((i * 2654435761u) >> 13);
    doc.AddLayer("noise", noise);

    const std::vector<std::uint8_t> saved = doc.Save();

    EXPECT_LT(saved.size(), noise.bytes.size() + 4000);
    EXPECT_EQ(ffpsd::Document::Parse(saved).GetLayerByIndex(0)->GetPixels().bytes, noise.bytes);
}

TEST(DocumentSaveTest, ARowTooLongForATwoByteCountIsWrittenRaw)
{
    // 20000 float samples are 80000 bytes a row: noise does not pack below 65535, the zeros do.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 20000, 2);
    ffpsd::Image image = Pattern(20000, 2, 1, 32);
    std::fill(image.bytes.begin() + 80000, image.bytes.end(), std::uint8_t{0});
    doc.AddLayer("wide", image);

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, image.bytes);
}

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
