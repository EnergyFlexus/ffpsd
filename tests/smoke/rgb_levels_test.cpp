// Every value of photoshop/rgb_levels.psd as tests/scripts/dump_psd.py reads it, and what writing keeps of them.
#include "support/test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace ffpsd_test;

namespace
{
    struct Stored
    {
        std::size_t size;
        std::uint64_t fnv;
    };

    struct Resource
    {
        std::uint16_t id;
        Stored data;
    };

    struct Block
    {
        char key[5];
        Stored data;
    };

    struct Plane
    {
        std::uint64_t sum;
        std::uint64_t fnv;
    };

    constexpr std::uint64_t kFileSize = 489899;
    constexpr std::uint64_t kFileFnv = 0x809f212a6a6f65b7u;
    constexpr std::uint32_t kWidth = 1890;
    constexpr std::uint32_t kHeight = 1417;

    // In file order, every name empty.
    constexpr Resource kResources[] = {
        {1061, {16, 0x88201fb960ff6465u}},   {1060, {13876, 0x90476985c1d3a2edu}}, {1082, {247, 0xdf4f5eeaafb64002u}},
        {1083, {557, 0xec70fc4aa1b74fbfu}},  {1005, {16, 0x7cd7a59a000b7971u}},    {1062, {14, 0x01ebb00be3bdd152u}},
        {1037, {4, 0x4d25ac7f9dce6fb7u}},    {1049, {4, 0x4d25687f9dcdfc2bu}},     {1011, {9, 0xe604813a2490280cu}},
        {10000, {10, 0xd668b06e8c5bf614u}},  {1013, {72, 0x18bae883c916c581u}},    {1016, {112, 0xd7fabbcd806b57c5u}},
        {1024, {2, 0x08328707b4eb6e3au}},    {1026, {4, 0x4d25767f9dce13f5u}},     {1072, {2, 0x082f2307b4e88e77u}},
        {1069, {6, 0xcb3618cfac4e0527u}},    {1032, {16, 0x2eedd863bcbb4382u}},    {1092, {16, 0xf66e13281806203fu}},
        {1097, {4, 0x4d25767f9dce13f5u}},    {1054, {4, 0x4d25767f9dce13f5u}},     {1050, {843, 0x7430f8ad0304c1f9u}},
        {1064, {12, 0x7eeb0eabd8ce58a0u}},   {1041, {1, 0xaf63bc4c8601b62cu}},     {1044, {4, 0x4d25737f9dce0edcu}},
        {1036, {3386, 0x6b3d035fc74a12b3u}}, {1057, {87, 0xb3a133c73d8594d5u}},    {1058, {306, 0x9b76c6a421f2b366u}}};

    constexpr Block kBackgroundBlocks[] = {{"luni", {12, 0x43ff96afb55fe1edu}}, {"lnsr", {4, 0x8816ae9b9f9af474u}},
                                           {"lyid", {4, 0x4d25757f9dce1242u}},  {"clbl", {4, 0xad2aca7747985764u}},
                                           {"infx", {4, 0x4d25767f9dce13f5u}},  {"knko", {4, 0x4d25767f9dce13f5u}},
                                           {"lspf", {4, 0x4d25797f9dce190eu}},  {"lclr", {8, 0xa8c7f832281a39c5u}},
                                           {"shmd", {72, 0x5ce3ce871eff2dafu}}, {"fxrp", {16, 0x88201fb960ff6465u}}};

    constexpr Block kLevelsBlocks[] = {
        {"levl", {632, 0xa2e2e1243c338566u}}, {"luni", {20, 0x864a60dfc089b234u}}, {"lnsr", {4, 0x0bca0c91195d8d6du}},
        {"lyid", {4, 0x4d25737f9dce0edcu}},   {"clbl", {4, 0xad2aca7747985764u}},  {"infx", {4, 0x4d25767f9dce13f5u}},
        {"knko", {4, 0x4d25767f9dce13f5u}},   {"lspf", {4, 0x4d25767f9dce13f5u}},  {"lclr", {8, 0xa8c7f832281a39c5u}},
        {"shmd", {72, 0x5d8022871f83ff0du}},  {"fxrp", {16, 0x88201fb960ff6465u}}};

    constexpr Block kSectionBlocks[] = {{"Patt", {0, 0xcbf29ce484222325u}},   {"CAI ", {77, 0x21f9ae263b0166bau}},
                                        {"OCIO", {172, 0x2700bc2e57347752u}}, {"GenI", {84, 0xf5e31de68e39d017u}},
                                        {"FMsk", {12, 0x5573a365a84623c1u}},  {"cinf", {410, 0x6767f23796513ee6u}}};

    constexpr Plane kBackgroundPlanes[] = {
        {613051406, 0x8c99fc77735fe575u}, {675524991, 0xd8a3f567293a0b3eu}, {646754204, 0x410d65fa2aab9d97u}};

    // The background with the Levels applied.
    constexpr Plane kCompositePlanes[] = {
        {612985261, 0xdc30c4a089111436u}, {682923150, 0x978207291fe19333u}, {640542472, 0x0bf1dab859ff58ffu}};

    // Input floor, input ceiling, output floor, output ceiling; every gamma 1.
    constexpr std::uint16_t kLevelsRecords[4][4] = {{70, 200, 0, 255}, {10, 245, 0, 255}, {20, 250, 0, 255}, {10, 250, 0, 255}};

    template <class Owner, std::size_t N> void ExpectBlocks(const Owner& owner, const Block (&expected)[N], const char* what)
    {
        ASSERT_EQ(owner.GetTaggedBlockCount(), N) << what;
        for (std::size_t i = 0; i < N; ++i)
        {
            const ffpsd::TaggedBlock* block = owner.GetTaggedBlockByIndex(i);
            EXPECT_EQ(block->signature, Fourcc("8BIM")) << what << " block " << i;
            EXPECT_EQ(block->key, Fourcc(expected[i].key)) << what << " block " << i;
            EXPECT_EQ(block->data.size(), expected[i].data.size) << what << " block " << expected[i].key;
            EXPECT_EQ(Fnv1a64(block->data), expected[i].data.fnv) << what << " block " << expected[i].key;
        }
    }

    template <std::size_t N> void ExpectPlanes(const ffpsd::Image& image, const Plane (&expected)[N], const char* what)
    {
        ASSERT_EQ(image.width, kWidth) << what;
        ASSERT_EQ(image.height, kHeight) << what;
        ASSERT_EQ(image.channel_count, N) << what;
        ASSERT_EQ(image.depth, 8u) << what;

        const std::size_t plane = std::size_t{kWidth} * kHeight;
        for (std::size_t c = 0; c < N; ++c)
        {
            EXPECT_EQ(PlaneSum(image, c), expected[c].sum) << what << " plane " << c;
            EXPECT_EQ(Fnv1a64(image.bytes.data() + c * plane, plane), expected[c].fnv) << what << " plane " << c;
        }
    }

    void ExpectResources(const ffpsd::Document& doc)
    {
        ASSERT_EQ(doc.GetImageResourceCount(), std::size(kResources));
        for (std::size_t i = 0; i < std::size(kResources); ++i)
        {
            const ffpsd::ImageResource* resource = doc.GetImageResourceByIndex(i);
            EXPECT_EQ(resource->id, kResources[i].id) << "resource " << i;
            EXPECT_EQ(resource->name, "") << "resource " << kResources[i].id;
            EXPECT_EQ(resource->data.size(), kResources[i].data.size) << "resource " << kResources[i].id;
            EXPECT_EQ(Fnv1a64(resource->data), kResources[i].data.fnv) << "resource " << kResources[i].id;
        }

        // 1005: 0x012BFFFE, just under 300 pixels per inch, set up in centimeters.
        const ffpsd::ResolutionInfo resolution = doc.GetResolutionInfo();
        EXPECT_DOUBLE_EQ(resolution.horizontal, 0x012BFFFE / 65536.0);
        EXPECT_DOUBLE_EQ(resolution.vertical, 0x012BFFFE / 65536.0);
        EXPECT_EQ(resolution.horizontal_unit, 2);
        EXPECT_EQ(resolution.vertical_unit, 2);
        EXPECT_EQ(resolution.width_unit, 2);
        EXPECT_EQ(resolution.height_unit, 2);

        const ffpsd::VersionInfo version = doc.GetVersionInfo();
        EXPECT_EQ(version.version, 1u);
        EXPECT_TRUE(version.has_real_merged_data);
        EXPECT_EQ(version.writer_name, "Adobe Photoshop");
        EXPECT_EQ(version.reader_name, "Adobe Photoshop 2026");
        EXPECT_EQ(version.file_version, 1u);
        EXPECT_TRUE(doc.HasRealMergedData());
    }

    void ExpectBackground(const ffpsd::Layer& layer)
    {
        EXPECT_EQ(layer.GetName(), kBackgroundName);
        EXPECT_EQ(layer.GetKind(), ffpsd::LayerKind::kRaster);
        EXPECT_TRUE(layer.IsBackground());
        EXPECT_TRUE(layer.IsVisible());
        EXPECT_EQ(layer.GetOpacity(), 255u);
        EXPECT_EQ(layer.GetBlendKey(), Fourcc("norm"));
        EXPECT_EQ(layer.GetAdjustmentKey(), 0u);
        EXPECT_FALSE(layer.GetAdjustment<ffpsd::LevelsInfo>().has_value());
        EXPECT_EQ(LayerId(layer), 1u);

        const ffpsd::Rect bounds = layer.GetBounds();
        EXPECT_EQ(bounds.top, 0);
        EXPECT_EQ(bounds.left, 0);
        EXPECT_EQ(bounds.bottom, 1417);
        EXPECT_EQ(bounds.right, 1890);

        ExpectBlocks(layer, kBackgroundBlocks, "background");

        // No channel -1: a background has no transparency.
        ExpectPlanes(layer.GetPixels(), kBackgroundPlanes, "background");
    }

    void ExpectLevels(const ffpsd::Layer& layer)
    {
        EXPECT_EQ(layer.GetName(), kLevelsName);
        EXPECT_EQ(layer.GetKind(), ffpsd::LayerKind::kAdjustment);
        EXPECT_FALSE(layer.IsBackground());
        EXPECT_TRUE(layer.IsVisible());
        EXPECT_EQ(layer.GetOpacity(), 255u);
        EXPECT_EQ(layer.GetBlendKey(), Fourcc("norm"));
        EXPECT_EQ(layer.GetAdjustmentKey(), Fourcc("levl"));
        EXPECT_EQ(LayerId(layer), 3u);

        const ffpsd::Rect bounds = layer.GetBounds();
        EXPECT_EQ(bounds.top, 0);
        EXPECT_EQ(bounds.left, 0);
        EXPECT_EQ(bounds.bottom, 0);
        EXPECT_EQ(bounds.right, 0);

        ExpectBlocks(layer, kLevelsBlocks, "levels");
        EXPECT_TRUE(layer.GetPixels().bytes.empty());

        const std::optional<ffpsd::LevelsInfo> levels = layer.GetAdjustment<ffpsd::LevelsInfo>();
        ASSERT_TRUE(levels.has_value());
        ASSERT_EQ(levels->channels.size(), 4u);
        for (std::size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(levels->channels[i].input_floor, kLevelsRecords[i][0]) << "record " << i;
            EXPECT_EQ(levels->channels[i].input_ceiling, kLevelsRecords[i][1]) << "record " << i;
            EXPECT_EQ(levels->channels[i].output_floor, kLevelsRecords[i][2]) << "record " << i;
            EXPECT_EQ(levels->channels[i].output_ceiling, kLevelsRecords[i][3]) << "record " << i;
            EXPECT_DOUBLE_EQ(levels->channels[i].gamma, 1.0) << "record " << i;
        }
    }

    // Every value the API shows, on the file itself and on whatever it was written into.
    void ExpectTheFile(const ffpsd::Document& doc, bool psb)
    {
        EXPECT_EQ(doc.IsPsb(), psb);
        EXPECT_EQ(doc.GetWidth(), kWidth);
        EXPECT_EQ(doc.GetHeight(), kHeight);
        EXPECT_EQ(doc.GetChannelCount(), 3u);
        EXPECT_EQ(doc.GetDepth(), 8u);
        EXPECT_EQ(doc.GetColorMode(), ffpsd::ColorMode::kRgb);

        ExpectResources(doc);

        ASSERT_EQ(doc.GetLayerCount(), 2u);
        ExpectBackground(*doc.GetLayerByIndex(0));
        ExpectLevels(*doc.GetLayerByIndex(1));

        ExpectBlocks(doc, kSectionBlocks, "section 4");
        ExpectPlanes(doc.GetMergedImage(), kCompositePlanes, "composite");
    }

    ffpsd::Document Reparsed(const std::vector<std::uint8_t>& bytes)
    {
        return ffpsd::Document::Parse(bytes);
    }
} // namespace

TEST(SmokeRgbLevelsTest, EveryValueIsTheDumps)
{
    const std::vector<std::uint8_t> file = ReadFile(kRgbLevelsPsd);
    ASSERT_EQ(file.size(), kFileSize);
    ASSERT_EQ(Fnv1a64(file), kFileFnv);

    ExpectTheFile(ffpsd::Document::Parse(file), false);
}

TEST(SmokeRgbLevelsTest, SavingUnchangedGivesTheFileBack)
{
    // Byte for byte, so what the API does not show survives too: flags, masks, blending ranges, legacy names.
    const std::vector<std::uint8_t> saved = ffpsd::Document::Open(kRgbLevelsPsd).Save();

    EXPECT_EQ(saved.size(), kFileSize);
    EXPECT_EQ(Fnv1a64(saved), kFileFnv);
}

TEST(SmokeRgbLevelsTest, RawAndBackKeepsEveryValue)
{
    const std::vector<std::uint8_t> raw = ffpsd::Document::Open(kRgbLevelsPsd).Save(ffpsd::Compression::kRaw);
    const ffpsd::Document from_raw = Reparsed(raw);
    ExpectTheFile(from_raw, false);

    // Unpacked, the composite alone is three planes of 1890 x 1417.
    EXPECT_GT(raw.size(), std::size_t{kWidth} * kHeight * 3);

    ExpectTheFile(Reparsed(from_raw.Save()), false);
}

TEST(SmokeRgbLevelsTest, APsbKeepsEveryValueAndComesBackAsTheFile)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    doc.SetPsb(true);
    ffpsd::Document psb = Reparsed(doc.Save());
    ExpectTheFile(psb, true);

    psb.SetPsb(false);
    const std::vector<std::uint8_t> back = psb.Save();

    EXPECT_EQ(back.size(), kFileSize);
    EXPECT_EQ(Fnv1a64(back), kFileFnv);
}

TEST(SmokeRgbLevelsTest, EditsChangeOnlyWhatTheyTouch)
{
    ffpsd::Document doc = ffpsd::Document::Open(kRgbLevelsPsd);
    ffpsd::Layer* background = doc.GetLayerByIndex(0);
    ffpsd::Layer* levels = doc.GetLayerByIndex(1);

    ffpsd::LevelsInfo changed = *levels->GetAdjustment<ffpsd::LevelsInfo>();
    changed.channels[0].input_floor = 80;
    changed.channels[2].gamma = 1.5;
    levels->SetAdjustment(changed);
    levels->SetVisible(false);
    background->SetOpacity(128);

    const ffpsd::Document back = Reparsed(doc.Save());
    const ffpsd::Layer* background_back = back.GetLayerByIndex(0);
    const ffpsd::Layer* levels_back = back.GetLayerByIndex(1);

    // What was changed.
    const ffpsd::LevelsInfo levels_read = *levels_back->GetAdjustment<ffpsd::LevelsInfo>();
    EXPECT_EQ(levels_read.channels[0].input_floor, 80u);
    EXPECT_DOUBLE_EQ(levels_read.channels[2].gamma, 1.5);
    EXPECT_FALSE(levels_back->IsVisible());
    EXPECT_EQ(background_back->GetOpacity(), 128u);

    // The composite is now stale; the flag byte says so, and the names of the writer stay.
    EXPECT_FALSE(back.HasRealMergedData());
    EXPECT_EQ(back.GetVersionInfo().writer_name, "Adobe Photoshop");

    // The rest of the Levels records, and everything else, as the file has it.
    for (std::size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(levels_read.channels[i].input_ceiling, kLevelsRecords[i][1]) << "record " << i;
        if (i != 0)
            EXPECT_EQ(levels_read.channels[i].input_floor, kLevelsRecords[i][0]) << "record " << i;
        if (i != 2)
            EXPECT_DOUBLE_EQ(levels_read.channels[i].gamma, 1.0) << "record " << i;
    }
    ExpectBlocks(*background_back, kBackgroundBlocks, "background");
    ExpectPlanes(background_back->GetPixels(), kBackgroundPlanes, "background");
    EXPECT_TRUE(background_back->IsBackground());
    EXPECT_EQ(levels_back->GetName(), kLevelsName);
    ExpectBlocks(back, kSectionBlocks, "section 4");
    ExpectPlanes(back.GetMergedImage(), kCompositePlanes, "composite");

    ASSERT_EQ(back.GetImageResourceCount(), std::size(kResources));
    for (std::size_t i = 0; i < std::size(kResources); ++i)
    {
        if (kResources[i].id != 1057)
            EXPECT_EQ(Fnv1a64(back.GetImageResourceByIndex(i)->data), kResources[i].data.fnv) << "resource " << kResources[i].id;
    }
}
