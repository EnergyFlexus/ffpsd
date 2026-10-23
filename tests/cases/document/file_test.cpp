#include "support/test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentFileTest, PhotoshopFilesComeBackByteForByteFromBytesOrAPath)
{
    for (const std::string& path : {kGrayscalePsd, kRgbPsd, kRgbLevelsPsd, kGrayscaleLevelsPsd})
    {
        const std::vector<std::uint8_t> original = ReadFile(path);
        ASSERT_FALSE(original.empty()) << path;

        const std::vector<std::uint8_t> saved = ffpsd::Document::Parse(original.data(), original.size()).Save();

        ASSERT_EQ(saved.size(), original.size()) << path;
        EXPECT_TRUE(saved == original) << path;
        EXPECT_EQ(ffpsd::Document::Open(path).Save(), saved) << path;
    }

    // A path is UTF-8 whatever the system's code page; "test" in Cyrillic, written as bytes for any compiler.
    const ffpsd::Document doc = ffpsd::Document::Open(kGrayscalePsd);
    for (const char* name : {"ffpsd_save_test.psd", "ffpsd_\xD1\x82\xD0\xB5\xD1\x81\xD1\x82.psd"})
    {
        const TempFile file(name);
        doc.Save(file.Utf8());
        EXPECT_EQ(ReadFile(file.Path()), ReadFile(kGrayscalePsd)) << name;
        EXPECT_EQ(ffpsd::Document::Open(file.Utf8()).Save(), doc.Save()) << name;
    }

    const TempFile unwritable("no_such_directory/out.psd");
    EXPECT_THROW(doc.Save(unwritable.Utf8()), std::system_error);
    EXPECT_THROW(ffpsd::Document::Open(DataFile("no_such_file.psd")), std::filesystem::filesystem_error);
}

TEST(DocumentFileTest, WhatNoPsdHoldsIsRefused)
{
    const std::vector<std::uint8_t> other = {'8', 'B', 'P', 'X', 0, 1, 0, 0, 0, 0, 0, 0, 0, 3};
    std::vector<std::uint8_t> cut = ReadFile(kRgbPsd);
    cut.resize(cut.size() / 3);

    // Big endian depth at bytes 22 and 23, color mode at 24 and 25.
    std::vector<std::uint8_t> unknown_mode = ReadFile(kRgbPsd);
    unknown_mode[25] = 5;
    std::vector<std::uint8_t> one_bit_rgb = ReadFile(kRgbPsd);
    one_bit_rgb[23] = 1;

    for (const std::vector<std::uint8_t>& bytes : {std::vector<std::uint8_t>(), other, cut, unknown_mode, one_bit_rgb})
        EXPECT_THROW(ffpsd::Document::Parse(bytes), std::runtime_error) << bytes.size() << " bytes";
}

TEST(DocumentFileTest, BlocksArePaddedAsPhotoshopWritesThemAndReadEitherWay)
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
    EXPECT_EQ(BlockData(*back.GetLayerByIndex(0), "abcd"), (std::vector<std::uint8_t>{4, 5, 6, 7, 8, 0, 0, 0}));

    // 'luni' of "Layer 1" is 18 bytes; Photoshop calls the file incompatible unless the padding is in the length.
    std::vector<std::uint8_t> luni = Ascii("8BIMluni");
    luni.insert(luni.end(), {0, 0, 0, 20});
    const std::vector<std::size_t> at = Find(saved, luni);
    ASSERT_FALSE(at.empty());
    EXPECT_EQ(std::string(saved.begin() + at[0] + 12 + 20, saved.begin() + at[0] + 12 + 24), "8BIM");

    // Without the 2 bytes that pad a 6 byte section block after its length, the next block starts right after it.
    ffpsd::Document blocks = NewDocument();
    blocks.SetTaggedBlock(Block("aaaa", {1, 2, 3, 4, 5, 6}));
    blocks.SetTaggedBlock(Block("bbbb", {7, 8, 9, 10}));
    std::vector<std::uint8_t> bytes = blocks.Save();
    const std::vector<std::size_t> block = Find(bytes, Ascii("8BIMaaaa"));
    ASSERT_EQ(block.size(), 1u);
    bytes.erase(bytes.begin() + block[0] + 12 + 6, bytes.begin() + block[0] + 12 + 8);

    // The layer and mask section, after the header, the color mode data and the resources, is 2 shorter.
    const std::size_t resources = 26 + 4 + BigEndianU32(bytes, 26);
    const std::size_t section = resources + 4 + BigEndianU32(bytes, resources);
    const std::vector<std::uint8_t> length = BigEndianBytes(BigEndianU32(bytes, section) - 2);
    std::copy(length.begin(), length.end(), bytes.begin() + static_cast<std::ptrdiff_t>(section));

    const ffpsd::Document unpadded = ffpsd::Document::Parse(bytes);
    ASSERT_EQ(unpadded.GetTaggedBlockCount(), 2u);
    EXPECT_EQ(unpadded.GetTaggedBlockByKey(Fourcc("aaaa"))->data, (std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(unpadded.GetTaggedBlockByKey(Fourcc("bbbb"))->data, (std::vector<std::uint8_t>{7, 8, 9, 10}));
}

TEST(DocumentFileTest, TheCompositeStaysOrIsWrittenBlank)
{
    ffpsd::Document photoshop = ffpsd::Document::Open(kRgbPsd);
    photoshop.RemoveLayer(1);
    photoshop.RemoveLayer(0);
    const ffpsd::Document without_layers = RoundTrip(photoshop);
    EXPECT_EQ(without_layers.GetLayerCount(), 0u);
    EXPECT_EQ(PlaneSum(without_layers.GetMergedImage(), 0), 183326186u);

    // A document made from scratch has no composite of its own, in either compression.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale);
    doc.AddLayer("one", Pattern(4, 3, 2));
    for (const ffpsd::Compression compression : {ffpsd::Compression::kRaw, ffpsd::Compression::kRle})
    {
        const ffpsd::Document back = RoundTrip(doc, compression);
        EXPECT_EQ(back.GetColorMode(), ffpsd::ColorMode::kGrayscale);
        EXPECT_EQ(back.GetWidth(), 4u);
        EXPECT_EQ(back.GetHeight(), 3u);
        ASSERT_EQ(back.GetLayerCount(), 1u);
        EXPECT_EQ(Pixels(back, 0), Pattern(4, 3, 2).bytes);
        EXPECT_EQ(back.GetMergedImage().bytes, ffpsd::Bytes(4 * 3, 0));
    }
}

TEST(DocumentFileTest, DeepLayersGoIntoTheirOwnBlock)
{
    // 16 and 32 bit layers go into 'Lr16' and 'Lr32'; the samples stay in native order.
    for (const std::uint16_t depth : {std::uint16_t{16}, std::uint16_t{32}})
    {
        ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kRgb, depth);
        doc.AddLayer("deep", Pattern(3, 2, 4, depth));
        doc.SetMergedImage(Pattern(4, 3, 3, depth));

        const ffpsd::Document back = RoundTrip(doc);

        EXPECT_NE(back.GetTaggedBlockByKey(depth == 16 ? Fourcc("Lr16") : Fourcc("Lr32")), nullptr);
        ASSERT_EQ(back.GetLayerCount(), 1u);
        EXPECT_EQ(Pixels(back, 0), Pattern(3, 2, 4, depth).bytes);
        EXPECT_EQ(back.GetMergedImage().bytes, Pattern(4, 3, 3, depth).bytes);
    }
}

TEST(DocumentFileTest, APsbKeepsEveryPixelAndBlock)
{
    // 'cinf' is not on the list of long keys, but Photoshop signs it 8B64 in a PSB and gives it 8 bytes of length.
    ffpsd::TaggedBlock block = Block("cinf", {1, 2, 3, 4, 5, 6, 7, 8});
    block.signature = Fourcc("8B64");

    ffpsd::Document doc = NewDocument();
    doc.SetPsb(true);
    doc.AddLayer("one", Pattern(3, 2, 4))->SetTaggedBlock(block);
    doc.SetTaggedBlock(block);
    const std::vector<std::uint8_t> file = doc.Save();

    std::vector<std::uint8_t> written = Ascii("8B64cinf");
    written.insert(written.end(), {0, 0, 0, 0, 0, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8});
    EXPECT_EQ(Find(file, written).size(), 2u);

    const ffpsd::Document back = ffpsd::Document::Parse(file);
    EXPECT_TRUE(back.IsPsb());
    EXPECT_EQ(Pixels(back, 0), Pattern(3, 2, 4).bytes);
    EXPECT_EQ(back.GetMergedImage().bytes.size(), 4u * 3 * 3);
    EXPECT_EQ(BlockData(*back.GetLayerByIndex(0), "cinf"), block.data);
    EXPECT_EQ(back.GetTaggedBlockByKey(Fourcc("cinf"))->data, block.data);
}

TEST(DocumentFileTest, SwitchingTheFormatRewritesOnlyTheRowCounts)
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

    // 30000 floats, three quarters noise: a row packs to about 91000 bytes, past a PSD count's 65535, so it is packed again.
    ffpsd::Document doc = NewDocument(ffpsd::ColorMode::kGrayscale, 32, 30000, 1);
    doc.SetPsb(true);
    ffpsd::Image image = Pattern(30000, 1, 1, 32);
    std::fill(image.bytes.begin(), image.bytes.end(), std::uint8_t{0});
    std::uint32_t state = 1;
    for (std::size_t i = 0; i < image.bytes.size() / 4 * 3; ++i)
    {
        state = state * 1664525u + 1013904223u;
        image.bytes[i] = static_cast<std::uint8_t>(state >> 24);
    }
    doc.AddLayer("long rows", image);

    doc.SetPsb(false);

    EXPECT_EQ(Pixels(RoundTrip(doc), 0), WithOpaqueAlpha(image).bytes);
}
