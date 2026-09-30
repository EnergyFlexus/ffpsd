#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

TEST(DocumentParseTest, BytesAndPathGiveTheSameDocument)
{
    const std::vector<std::uint8_t> bytes = ReadFile(kGrayscalePsd);
    const std::filesystem::path missing = std::filesystem::path(FFPSD_TEST_DATA_DIR) / "no_such_file.psd";

    const ffpsd::Document from_path = ffpsd::Document::Open(kGrayscalePsd);
    const ffpsd::Document from_pointer = ffpsd::Document::Parse(bytes.data(), bytes.size());

    EXPECT_EQ(from_pointer.Save(), from_path.Save());
    EXPECT_THROW(ffpsd::Document::Open(missing.string()), std::filesystem::filesystem_error);
}

TEST(DocumentParseTest, WhatIsNotAPsdIsRefused)
{
    const std::vector<std::uint8_t> empty;
    const std::vector<std::uint8_t> other = {'8', 'B', 'P', 'X', 0, 1, 0, 0, 0, 0, 0, 0, 0, 3};
    std::vector<std::uint8_t> cut = ReadFile(kRgbPsd);
    cut.resize(cut.size() / 3);

    EXPECT_THROW(ffpsd::Document::Parse(empty), std::runtime_error);
    EXPECT_THROW(ffpsd::Document::Parse(other), std::runtime_error);
    EXPECT_THROW(ffpsd::Document::Parse(cut), std::runtime_error);
}

TEST(DocumentParseTest, ABlockPaddedToTwoAsTheSpecificationSaysIsReadToo)
{
    // Without the 2 bytes that pad the 6 byte block after its length, the next block starts right after it.
    ffpsd::Document doc = NewDocument();
    doc.SetTaggedBlock(Block("aaaa", {1, 2, 3, 4, 5, 6}));
    doc.SetTaggedBlock(Block("bbbb", {7, 8, 9, 10}));
    std::vector<std::uint8_t> bytes = doc.Save();

    const std::uint8_t key[] = {'8', 'B', 'I', 'M', 'a', 'a', 'a', 'a'};
    const auto block = std::search(bytes.begin(), bytes.end(), std::begin(key), std::end(key));
    ASSERT_NE(block, bytes.end());
    bytes.erase(block + 12 + 6, block + 12 + 8);

    // The layer and mask section, after the header, the color mode data and the resources, is 2 shorter.
    const std::size_t resources = 26 + 4 + BigEndianU32(std::vector<std::uint8_t>(bytes.begin() + 26, bytes.begin() + 30));
    const std::size_t section =
        resources + 4 + BigEndianU32(std::vector<std::uint8_t>(bytes.begin() + resources, bytes.begin() + resources + 4));
    const std::vector<std::uint8_t> length =
        BigEndianBytes(BigEndianU32(std::vector<std::uint8_t>(bytes.begin() + section, bytes.begin() + section + 4)) - 2);
    std::copy(length.begin(), length.end(), bytes.begin() + static_cast<std::ptrdiff_t>(section));

    const ffpsd::Document back = ffpsd::Document::Parse(bytes);

    ASSERT_EQ(back.GetTaggedBlockCount(), 2u);
    EXPECT_EQ(back.GetTaggedBlockByKey(Fourcc("aaaa"))->data, (std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(back.GetTaggedBlockByKey(Fourcc("bbbb"))->data, (std::vector<std::uint8_t>{7, 8, 9, 10}));
}
