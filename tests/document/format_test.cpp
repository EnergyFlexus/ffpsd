#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentFormatTest, APsbRoundTrips)
{
    ffpsd::Document doc = NewDocument();
    doc.SetPsb(true);
    doc.AddLayer("one", Pattern(3, 2, 4));

    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_TRUE(back.IsPsb());
    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    EXPECT_EQ(back.GetMergedImage().bytes.size(), 4u * 3 * 3);
}

TEST(DocumentFormatTest, SwitchingTheFormatKeepsEveryPixel)
{
    // Photoshop's RLE, with 2 byte row counts, through a PSB with 4 byte ones and back.
    const ffpsd::Document original = ffpsd::Document::Open(kRgbPsd);
    ffpsd::Document doc = ffpsd::Document::Open(kRgbPsd);

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

TEST(DocumentFormatTest, SwitchingTheFormatAndBackGivesTheSameFile)
{
    // Only the width of the row counts changes, so Photoshop's packed rows come back byte for byte.
    for (const std::string& path : {kRgbPsd, kGrayscalePsd, kRgbLevelsPsd})
    {
        const std::vector<std::uint8_t> original = ffpsd::Document::Open(path).Save();
        ffpsd::Document doc = ffpsd::Document::Open(path);

        doc.SetPsb(true);
        doc.SetPsb(false);

        EXPECT_EQ(doc.Save(), original) << path;
    }
}

TEST(DocumentFormatTest, APsbRowCountTooBigForAPsdIsPackedAgain)
{
    // 30000 floats, three quarters noise: a row packs to about 91000 bytes, past a PSD count's 65535.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 30000, 1);
    doc.SetPsb(true);

    ffpsd::Image image;
    image.width = 30000;
    image.height = 1;
    image.channel_count = 1;
    image.depth = 32;
    image.bytes.assign(image.GetSizeBytes(), 0);
    std::uint32_t state = 1;
    for (std::size_t i = 0; i < image.bytes.size() / 4 * 3; ++i)
    {
        state = state * 1664525u + 1013904223u;
        image.bytes[i] = static_cast<std::uint8_t>(state >> 24);
    }
    doc.AddLayer("long rows", image);

    doc.SetPsb(false);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, image.bytes);
}
