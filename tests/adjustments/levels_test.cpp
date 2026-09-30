#include "support/test_support.hpp"

#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace ffpsd_test;

namespace
{
    constexpr std::uint32_t kLevelsKey = Fourcc("levl");

    void ExpectIdentity(const ffpsd::LevelsInfo::Channel& channel)
    {
        EXPECT_EQ(channel.input_floor, 0u);
        EXPECT_EQ(channel.input_ceiling, 255u);
        EXPECT_EQ(channel.output_floor, 0u);
        EXPECT_EQ(channel.output_ceiling, 255u);
        EXPECT_DOUBLE_EQ(channel.gamma, 1.0);
    }
} // namespace

TEST(AdjustmentLevelsTest, PhotoshopsRecordsReadAndWriteBack)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    ffpsd::Layer* levels = doc.GetLayerByIndex(1);
    const std::vector<std::uint8_t> before = levels->GetTaggedBlockByKey(kLevelsKey)->data;

    // Set what Get gave writes Photoshop's bytes.
    levels->SetAdjustment(*levels->GetAdjustment<ffpsd::LevelsInfo>());
    EXPECT_EQ(levels->GetTaggedBlockByKey(kLevelsKey)->data, before);

    ffpsd::LevelsInfo info = *levels->GetAdjustment<ffpsd::LevelsInfo>();
    info.channels[0].gamma = 1.5;
    info.channels[3].output_ceiling = 240;
    levels->SetAdjustment(info);
    const ffpsd::LevelsInfo after = *levels->GetAdjustment<ffpsd::LevelsInfo>();
    EXPECT_DOUBLE_EQ(after.channels[0].gamma, 1.5);
    EXPECT_EQ(after.channels[3].output_ceiling, 240u);
    EXPECT_EQ(after.channels[1].input_floor, 10u);
    EXPECT_FALSE(doc.HasRealMergedData());

    // Set up as 25 to 237 on the Gray channel; record 0, every channel at once, stays the identity.
    const ffpsd::Document gray = ffpsd::Document::Open(kGrayscaleLevelsPsd);
    const std::optional<ffpsd::LevelsInfo> gray_info = gray.GetLayerByIndex(2)->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_TRUE(gray_info.has_value());
    ASSERT_EQ(gray_info->channels.size(), 2u);
    ExpectIdentity(gray_info->channels[0]);
    EXPECT_EQ(gray_info->channels[1].input_floor, 25u);
    EXPECT_EQ(gray_info->channels[1].input_ceiling, 237u);
    EXPECT_EQ(gray_info->channels[1].output_floor, 0u);
    EXPECT_EQ(gray_info->channels[1].output_ceiling, 255u);
    EXPECT_DOUBLE_EQ(gray_info->channels[1].gamma, 1.0);
}

TEST(AdjustmentLevelsTest, OnlyALevelsLayerTakesRecordsInPhotoshopsRanges)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    ffpsd::Layer* levels = doc.GetLayerByIndex(1);
    const std::vector<std::uint8_t> before = levels->GetTaggedBlockByKey(kLevelsKey)->data;

    // Only a Levels layer takes Levels.
    ffpsd::Layer* background = doc.GetLayerByIndex(0);
    EXPECT_FALSE(background->GetAdjustment<ffpsd::LevelsInfo>().has_value());
    EXPECT_THROW(background->SetAdjustment(ffpsd::LevelsInfo()), std::invalid_argument);
    EXPECT_EQ(background->GetTaggedBlockByKey(kLevelsKey), nullptr);

    const auto with = [](auto change) {
        ffpsd::LevelsInfo info;
        info.channels.resize(1);
        change(info.channels[0]);
        return info;
    };
    using Channel = ffpsd::LevelsInfo::Channel;
    EXPECT_THROW(levels->SetAdjustment(with([](Channel& c) { c.gamma = 10.0; })), std::invalid_argument);
    EXPECT_THROW(levels->SetAdjustment(with([](Channel& c) { c.gamma = 0.09; })), std::invalid_argument);
    EXPECT_THROW(levels->SetAdjustment(with([](Channel& c) { c.input_floor = 254; })), std::invalid_argument);
    EXPECT_THROW(levels->SetAdjustment(with([](Channel& c) { c.input_ceiling = 1; })), std::invalid_argument);
    EXPECT_THROW(levels->SetAdjustment(with([](Channel& c) { c.output_ceiling = 256; })), std::invalid_argument);
    EXPECT_THROW(
        levels->SetAdjustment(with([](Channel& c) {
            c.input_floor = 100;
            c.input_ceiling = 100;
        })),
        std::invalid_argument);
    EXPECT_EQ(levels->GetTaggedBlockByKey(kLevelsKey)->data, before);

    // The edges themselves pass.
    levels->SetAdjustment(with([](Channel& c) {
        c.input_floor = 253;
        c.input_ceiling = 255;
        c.gamma = 9.99;
    }));
    levels->SetAdjustment(with([](Channel& c) {
        c.input_ceiling = 2;
        c.gamma = 0.1;
    }));
}

TEST(AdjustmentLevelsTest, MissingRecordsAreTheIdentity)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    ffpsd::Layer* levels = doc.GetLayerByIndex(1);
    levels->SetAdjustment(ffpsd::LevelsInfo());
    const ffpsd::LevelsInfo info = *levels->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_EQ(info.channels.size(), 4u);
    for (const ffpsd::LevelsInfo::Channel& channel : info.channels)
        ExpectIdentity(channel);

    // A new layer has a record for every color channel and one for all of them.
    ffpsd::Document rgb = NewDocument();
    const ffpsd::LevelsInfo rgb_info = *rgb.AddAdjustmentLayer<ffpsd::LevelsInfo>("levels")->GetAdjustment<ffpsd::LevelsInfo>();
    ASSERT_EQ(rgb_info.channels.size(), 4u);
    for (const ffpsd::LevelsInfo::Channel& channel : rgb_info.channels)
        ExpectIdentity(channel);
    ffpsd::Document gray = NewDocument(ffpsd::ColorMode::kGrayscale);
    EXPECT_EQ(gray.AddAdjustmentLayer<ffpsd::LevelsInfo>("levels")->GetAdjustment<ffpsd::LevelsInfo>()->channels.size(), 2u);
}

TEST(AdjustmentLevelsTest, ANewLayerIsWhatPhotoshopWritesThroughSaving)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    const ffpsd::Layer* photoshops = doc.GetLayerByIndex(1);

    const ffpsd::Layer* added = doc.AddAdjustmentLayer("levels", *photoshops->GetAdjustment<ffpsd::LevelsInfo>());

    EXPECT_EQ(added->GetKind(), ffpsd::LayerKind::kAdjustment);
    EXPECT_EQ(ffpsd::LevelsInfo::kKey, kLevelsKey);
    EXPECT_EQ(added->GetAdjustmentKey(), kLevelsKey);
    EXPECT_EQ(added->GetBounds().GetWidth(), 0);
    EXPECT_TRUE(added->GetPixels().bytes.empty());
    EXPECT_EQ(added->GetTaggedBlockByKey(kLevelsKey)->data, photoshops->GetTaggedBlockByKey(kLevelsKey)->data);
    EXPECT_EQ(added->GetTaggedBlockByIndex(0)->key, kLevelsKey);
    EXPECT_NE(LayerId(*added), 0u);

    ffpsd::Document made = NewDocument();
    made.AddLayer("under", Pattern(4, 3, 3));
    ffpsd::LevelsInfo info;
    info.channels.resize(2);
    info.channels[1].input_floor = 30;
    made.AddAdjustmentLayer("levels", info);
    const ffpsd::Document back = ffpsd::Document::Parse(made.Save());
    ASSERT_EQ(back.GetLayerCount(), 2u);
    const ffpsd::Layer* levels = back.GetLayerByIndex(1);
    EXPECT_EQ(levels->GetName(), "levels");
    EXPECT_EQ(levels->GetKind(), ffpsd::LayerKind::kAdjustment);
    EXPECT_EQ(levels->GetAdjustment<ffpsd::LevelsInfo>()->channels[1].input_floor, 30u);
    ExpectIdentity(levels->GetAdjustment<ffpsd::LevelsInfo>()->channels[2]);
}
