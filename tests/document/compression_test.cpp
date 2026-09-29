#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentCompressionTest, RawAsksForUnpackedPixels)
{
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);

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

TEST(DocumentCompressionTest, RawAndRleGoBothWays)
{
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);
    const std::vector<std::uint8_t> raw = original.Save(ffpsd::Compression::kRaw);

    const std::vector<std::uint8_t> rle = ffpsd::Document::Parse(raw).Save();

    EXPECT_LT(rle.size(), raw.size());
    const ffpsd::Document back = ffpsd::Document::Parse(rle);
    EXPECT_EQ(back.GetMergedImage().bytes, original.GetMergedImage().bytes);
    EXPECT_EQ(back.GetLayerByIndex(1)->GetPixels().bytes, original.GetLayerByIndex(1)->GetPixels().bytes);
}

TEST(DocumentCompressionTest, ABlankCompositeComesInEitherCompression)
{
    ffpsd::Document doc = NewDocument();

    const ffpsd::Document raw = ffpsd::Document::Parse(doc.Save(ffpsd::Compression::kRaw));
    const ffpsd::Document rle = ffpsd::Document::Parse(doc.Save(ffpsd::Compression::kRle));

    EXPECT_EQ(raw.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3 * 3, 0));
    EXPECT_EQ(rle.GetMergedImage().bytes, std::vector<std::uint8_t>(4 * 3 * 3, 0));
}

TEST(DocumentCompressionTest, FlatPixelsAreWrittenSmall)
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

TEST(DocumentCompressionTest, PixelsThatDoNotPackAreWrittenRaw)
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

TEST(DocumentCompressionTest, ARowTooLongForATwoByteCountIsWrittenRaw)
{
    // 20000 float samples are 80000 bytes a row: noise does not pack below 65535, the zeros do.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 20000, 2);
    ffpsd::Image image = Pattern(20000, 2, 1, 32);
    std::fill(image.bytes.begin() + 80000, image.bytes.end(), std::uint8_t{0});
    doc.AddLayer("wide", image);

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, image.bytes);
}
