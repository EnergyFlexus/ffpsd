#include "support/test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentFormatTest, APsbKeepsEveryPixel)
{
    ffpsd::Document made = NewDocument();
    made.SetPsb(true);
    made.AddLayer("one", Pattern(3, 2, 4));
    const ffpsd::Document made_back = ffpsd::Document::Parse(made.Save());
    EXPECT_TRUE(made_back.IsPsb());
    EXPECT_EQ(made_back.GetLayerByIndex(0)->GetPixels().bytes, Pattern(3, 2, 4).bytes);
    EXPECT_EQ(made_back.GetMergedImage().bytes.size(), 4u * 3 * 3);
}

TEST(DocumentFormatTest, APsbBlockSigned8B64HasAnEightByteLength)
{
    // 'cinf' is not on the list of long keys, but Photoshop signs it 8B64 in a PSB and gives it 8 bytes of length.
    ffpsd::TaggedBlock block;
    block.signature = Fourcc("8B64");
    block.key = Fourcc("cinf");
    block.data = {1, 2, 3, 4, 5, 6, 7, 8};

    ffpsd::Document doc = NewDocument();
    doc.SetPsb(true);
    doc.AddLayer("one", Pattern(3, 2, 4))->SetTaggedBlock(block);
    doc.SetTaggedBlock(block);
    const std::vector<std::uint8_t> file = doc.Save();

    const std::vector<std::uint8_t> written = {'8', 'B', '6', '4', 'c', 'i', 'n', 'f', 0, 0, 0, 0, 0, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    std::size_t found = 0;
    for (auto at = file.begin(); (at = std::search(at, file.end(), written.begin(), written.end())) != file.end(); ++at)
        ++found;
    EXPECT_EQ(found, 2u);

    const ffpsd::Document back = ffpsd::Document::Parse(file);
    EXPECT_EQ(back.GetLayerByIndex(0)->GetTaggedBlockByKey(Fourcc("cinf"))->data, block.data);
    EXPECT_EQ(back.GetTaggedBlockByKey(Fourcc("cinf"))->data, block.data);
}

TEST(DocumentFormatTest, SwitchingTheFormatAndBackGivesTheSameFile)
{
    // Only the width of the row counts changes, so Photoshop's packed rows come back byte for byte; masks too.
    for (const std::string& path : {kRgbPsd, kGrayscalePsd, kRgbLevelsPsd, kRgbMasksPsd})
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
    image.color_mode = ffpsd::ColorMode::kGrayscale;
    image.bytes = ffpsd::Bytes(image.GetSizeBytes(), 0);
    std::uint32_t state = 1;
    for (std::size_t i = 0; i < image.bytes.size() / 4 * 3; ++i)
    {
        state = state * 1664525u + 1013904223u;
        image.bytes[i] = static_cast<std::uint8_t>(state >> 24);
    }
    doc.AddLayer("long rows", image);

    doc.SetPsb(false);
    const ffpsd::Document back = ffpsd::Document::Parse(doc.Save());

    EXPECT_EQ(back.GetLayerByIndex(0)->GetPixels().bytes, WithOpaqueAlpha(image).bytes);
}
