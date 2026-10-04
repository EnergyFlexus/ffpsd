#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentCompressionTest, RawAndRleGiveTheSamePixels)
{
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);

    const std::vector<std::uint8_t> raw = original.Save(ffpsd::Compression::kRaw);
    const std::vector<std::uint8_t> rle = ffpsd::Document::Parse(raw).Save();

    // Section 5 ends the file: a compression field of 0, then every plane of 1890 x 1417 x 3.
    const std::size_t composite = std::size_t{1890} * 1417 * 3;
    ASSERT_GT(raw.size(), composite + 2);
    EXPECT_EQ(raw[raw.size() - composite - 2], 0u);
    EXPECT_EQ(raw[raw.size() - composite - 1], 0u);
    EXPECT_LT(rle.size(), raw.size());

    for (const std::vector<std::uint8_t>* bytes : {&raw, &rle})
    {
        const ffpsd::Document back = ffpsd::Document::Parse(*bytes);
        EXPECT_EQ(back.GetMergedImage().bytes, original.GetMergedImage().bytes);
        for (std::size_t i = 0; i < 2; ++i)
            EXPECT_EQ(back.GetLayerByIndex(i)->GetPixels().bytes, original.GetLayerByIndex(i)->GetPixels().bytes);
    }

    // Masks go raw too: 15 layer planes, 3 composite ones and masks of 1417 x 955 and twice 1417 x 703.
    const ffpsd::Document masks = ffpsd::Document::Open(kRgbMasksPsd);
    const std::vector<std::uint8_t> masks_raw = masks.Save(ffpsd::Compression::kRaw);
    EXPECT_GT(masks_raw.size(), std::size_t{1890} * 1417 * 18 + std::size_t{1417} * (955 + 2 * 703));
    const ffpsd::Document masks_back = ffpsd::Document::Parse(masks_raw);
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(masks_back.GetLayerByIndex(i)->GetPixels().bytes, masks.GetLayerByIndex(i)->GetPixels().bytes);
}

TEST(DocumentCompressionTest, RleIsKeptOnlyWhereItPacks)
{
    // Flat rows of 1000 bytes are 8 runs of 2 bytes plus a 2 byte count: 7000 rows of 18, against 7 MB raw.
    ffpsd::Document flat_doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 1000, 1000);
    ffpsd::Image flat = Pattern(1000, 1000, 4);
    std::fill(flat.bytes.begin(), flat.bytes.end(), std::uint8_t{200});
    flat_doc.AddLayer("flat", flat);
    ffpsd::Image merged = Pattern(1000, 1000, 3);
    std::fill(merged.bytes.begin(), merged.bytes.end(), std::uint8_t{200});
    flat_doc.SetMergedImage(merged);
    const std::vector<std::uint8_t> flat_saved = flat_doc.Save();
    EXPECT_LT(flat_saved.size(), 126000u + 1000u);
    const ffpsd::Document flat_back = ffpsd::Document::Parse(flat_saved);
    EXPECT_EQ(flat_back.GetLayerByIndex(0)->GetPixels().bytes, flat.bytes);
    EXPECT_EQ(flat_back.GetMergedImage().bytes, merged.bytes);

    // Every byte differs from its neighbour, so RLE would only grow them.
    ffpsd::Document noise_doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 64);
    ffpsd::Image noise = Pattern(64, 64, 4);
    for (std::size_t i = 0; i < noise.bytes.size(); ++i)
        noise.bytes[i] = static_cast<std::uint8_t>((i * 2654435761u) >> 13);
    noise_doc.AddLayer("noise", noise);
    const std::vector<std::uint8_t> noise_saved = noise_doc.Save();
    EXPECT_LT(noise_saved.size(), noise.bytes.size() + 4000);
    EXPECT_EQ(ffpsd::Document::Parse(noise_saved).GetLayerByIndex(0)->GetPixels().bytes, noise.bytes);

    // 20000 float samples are 80000 bytes a row: noise does not pack below a 2 byte count's 65535, the zeros do.
    ffpsd::Document wide_doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 20000, 2);
    ffpsd::Image wide = Pattern(20000, 2, 1, 32);
    std::fill(wide.bytes.begin() + 80000, wide.bytes.end(), std::uint8_t{0});
    wide_doc.AddLayer("wide", wide);
    EXPECT_EQ(ffpsd::Document::Parse(wide_doc.Save()).GetLayerByIndex(0)->GetPixels().bytes, WithOpaqueAlpha(wide).bytes);
}
