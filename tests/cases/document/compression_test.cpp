#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentCompressionTest, RawAndRleGiveTheSamePixels)
{
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);

    const std::vector<std::uint8_t> raw = original.Save(ffpsd::Compression::kRaw);
    const std::vector<std::uint8_t> rle = ffpsd::Document::Parse(raw).Save(ffpsd::Compression::kRleOrRaw);

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

TEST(DocumentCompressionTest, RleIsWrittenEvenWhereRawIsSmaller)
{
    // RLE only grows noise; kRle writes it all the same.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 64);
    const ffpsd::Image noise = Noise(64, 64, 4);
    doc.AddLayer("noise", noise);
    doc.SetMergedImage(Pattern(64, 64, 3));

    const std::vector<std::uint8_t> rle = doc.Save(ffpsd::Compression::kRle);
    EXPECT_GT(rle.size(), doc.Save(ffpsd::Compression::kRleOrRaw).size());

    const ffpsd::Document back = ffpsd::Document::Parse(rle);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, noise.bytes);
    EXPECT_EQ(back.GetMergedImage().bytes, Pattern(64, 64, 3).bytes);
}

TEST(DocumentCompressionTest, RleIsKeptOnlyWhereItPacks)
{
    // Flat rows of 1000 bytes are 8 runs of 2 bytes plus a 2 byte count: 7000 rows of 18, against 7 MB raw.
    ffpsd::Document flat_doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 1000, 1000);
    const ffpsd::Image flat = Flat(1000, 1000, 4, 200);
    const ffpsd::Image merged = Flat(1000, 1000, 3, 200);
    flat_doc.AddLayer("flat", flat);
    flat_doc.SetMergedImage(merged);
    const std::vector<std::uint8_t> flat_saved = flat_doc.Save();
    EXPECT_LT(flat_saved.size(), 126000u + 1000u);
    const ffpsd::Document flat_back = ffpsd::Document::Parse(flat_saved);
    EXPECT_EQ(flat_back.GetLayerByIndex(0)->GetPixels().bytes, flat.bytes);
    EXPECT_EQ(flat_back.GetMergedImage().bytes, merged.bytes);

    // Noise stays raw: a compression field and 64 x 64 bytes a channel.
    ffpsd::Document noise_doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 64);
    const ffpsd::Image noise = Noise(64, 64, 4);
    const ffpsd::Layer& added = *noise_doc.AddLayer("noise", noise);
    for (std::size_t i = 0; i < added.GetChannelCount(); ++i)
    {
        EXPECT_EQ(added.GetChannelByIndex(i).compression, ffpsd::ChannelCompression::kRaw) << i;
        EXPECT_EQ(added.GetChannelByIndex(i).size, 2u + 64 * 64) << i;
    }
    EXPECT_EQ(ffpsd::Document::Parse(noise_doc.Save()).GetLayerByIndex(0)->GetPixels().bytes, noise.bytes);

    // 20000 float samples are 80000 bytes a row: noise does not pack below a 2 byte count's 65535, the zeros do.
    ffpsd::Document wide_doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 20000, 2);
    ffpsd::Image wide = Pattern(20000, 2, 1, 32);
    std::fill(wide.bytes.begin() + 80000, wide.bytes.end(), std::uint8_t{0});
    wide_doc.AddLayer("wide", wide);
    EXPECT_EQ(ffpsd::Document::Parse(wide_doc.Save()).GetLayerByIndex(0)->GetPixels().bytes, WithOpaqueAlpha(wide).bytes);
}

TEST(DocumentCompressionTest, DefaultKeepsWhatIsStoredAndPacksWhatIsNewAsRleOrRaw)
{
    // Raw from the file stays raw, though RLE would pack it.
    const std::vector<std::uint8_t> raw = ffpsd::Document::Open(kRgbPsd).Save(ffpsd::Compression::kRaw);
    EXPECT_EQ(ffpsd::Document::Parse(raw).Save(), raw);

    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 64);
    doc.AddLayer("noise", Noise(64, 64, 4));
    doc.AddLayer("flat", Flat(64, 64, 4, 200));
    EXPECT_EQ(doc.Save(), doc.Save(ffpsd::Compression::kRleOrRaw));
    doc.SetMergedImage(Pattern(64, 64, 3));
    EXPECT_EQ(doc.Save(), doc.Save(ffpsd::Compression::kRleOrRaw));
}

TEST(DocumentCompressionTest, ChannelsTellHowTheyAreStored)
{
    const ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    EXPECT_EQ(photoshop.GetMergedCompression(), ffpsd::ChannelCompression::kRle);

    const ffpsd::Layer& layer = *photoshop.GetLayerByIndex(1);
    ASSERT_EQ(layer.GetChannelCount(), 4u);
    std::vector<std::int16_t> ids;
    for (std::size_t i = 0; i < layer.GetChannelCount(); ++i)
    {
        const ffpsd::ChannelInfo channel = layer.GetChannelByIndex(i);
        ids.push_back(channel.id);
        EXPECT_EQ(channel.compression, ffpsd::ChannelCompression::kRle);
        EXPECT_GT(channel.size, 2u);
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<std::int16_t>{-1, 0, 1, 2}));
    EXPECT_THROW(layer.GetChannelByIndex(4), std::out_of_range);

    // Written raw, the composite says so; a new document has none to tell about.
    EXPECT_EQ(ffpsd::Document::Parse(photoshop.Save(ffpsd::Compression::kRaw)).GetMergedCompression(), ffpsd::ChannelCompression::kRaw);
    EXPECT_EQ(NewDocument().GetMergedCompression(), std::nullopt);
}

TEST(DocumentCompressionTest, NoiseAtTheTopDoesNotKeepTheRestRaw)
{
    // The top 10% is noise, the rest flat: RLE still packs the layer to a fraction of its 4 x 64 x 160 bytes.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, 8, 64, 160);
    const ffpsd::Image noise = Noise(64, 160, 4);
    ffpsd::Image picture = Flat(64, 160, 4, 0);
    const std::size_t plane = std::size_t{64} * 160;
    for (std::size_t i = 0; i < picture.bytes.size(); ++i)
        if (i % plane < plane / 10)
            picture.bytes[i] = noise.bytes[i];
    const ffpsd::Layer& layer = *doc.AddLayer("top noise", picture);
    for (std::size_t i = 0; i < layer.GetChannelCount(); ++i)
        EXPECT_EQ(layer.GetChannelByIndex(i).compression, ffpsd::ChannelCompression::kRle) << layer.GetChannelByIndex(i).id;
    EXPECT_EQ(ffpsd::Document::Parse(doc.Save()).GetLayerByIndex(0)->GetPixels().bytes, picture.bytes);
}
