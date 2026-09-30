// The C API alone, as a C program sees it; c_header_check.c compiles the header as C.
#include "support/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ffpsd/c_api.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace ffpsd_test;

extern "C" int ffpsd_test_count_layers_from_c(const char* path);

namespace
{
    // Owns a document handle for one test.
    struct OpenDocument
    {
        ffpsd_document_t* doc = nullptr;

        explicit OpenDocument(const std::string& path)
        {
            EXPECT_EQ(ffpsd_document_open(path.c_str(), &doc), FFPSD_STATUS_OK) << ffpsd_last_error();
        }
        OpenDocument(const OpenDocument&) = delete;
        OpenDocument& operator=(const OpenDocument&) = delete;
        ~OpenDocument()
        {
            ffpsd_document_destroy(doc);
        }
    };

    ffpsd_layer_t* Layer(ffpsd_document_t* doc, size_t index)
    {
        ffpsd_layer_t* layer = nullptr;
        EXPECT_EQ(ffpsd_document_get_layer(doc, index, &layer), FFPSD_STATUS_OK) << ffpsd_last_error();
        return layer;
    }

    std::string Name(const ffpsd_layer_t* layer)
    {
        size_t length = 0;
        EXPECT_EQ(ffpsd_layer_get_name(layer, nullptr, 0, &length), FFPSD_STATUS_OK);
        std::string name(length, '\0');
        EXPECT_EQ(ffpsd_layer_get_name(layer, name.data(), name.size() + 1, &length), FFPSD_STATUS_OK);
        return name;
    }

    std::vector<std::uint8_t> Bytes(const ffpsd_image_t* image)
    {
        ffpsd_image_view_t view = {};
        EXPECT_EQ(ffpsd_image_get_view(image, &view), FFPSD_STATUS_OK);
        return std::vector<std::uint8_t>(view.data, view.data + view.size);
    }

    std::vector<std::uint8_t> Pixels(const ffpsd_layer_t* layer)
    {
        ffpsd_image_t* image = nullptr;
        EXPECT_EQ(ffpsd_layer_get_pixels(layer, &image), FFPSD_STATUS_OK) << ffpsd_last_error();
        std::vector<std::uint8_t> bytes = Bytes(image);
        ffpsd_image_destroy(image);
        return bytes;
    }

    ffpsd_image_view_t View(const ffpsd::Image& image)
    {
        return {image.width, image.height, image.channel_count, image.depth, image.bytes.data(), image.bytes.size()};
    }

    ffpsd_document_t* NewRgbDocument()
    {
        ffpsd_document_t* doc = nullptr;
        EXPECT_EQ(ffpsd_document_create_with(4, 3, FFPSD_COLOR_MODE_RGB, 8, &doc), FFPSD_STATUS_OK);
        return doc;
    }
} // namespace

TEST(CApiTest, TheHeaderIsCAndNullHandlesAreHarmless)
{
    EXPECT_EQ(ffpsd_test_count_layers_from_c(kRgbPsd.c_str()), 2);

    const std::string version = ffpsd_version();
    EXPECT_EQ(std::count(version.begin(), version.end(), '.'), 2);

    EXPECT_EQ(ffpsd_document_get_width(nullptr), 0u);
    EXPECT_EQ(ffpsd_document_get_layer_count(nullptr), 0u);
    EXPECT_EQ(ffpsd_layer_get_opacity(nullptr), 0u);
    EXPECT_EQ(ffpsd_buffer_get_data(nullptr), nullptr);
    EXPECT_EQ(ffpsd_version_info_get_writer_name(nullptr), nullptr);

    // Destroying NULL is allowed.
    ffpsd_document_destroy(nullptr);
    ffpsd_image_destroy(nullptr);
    ffpsd_buffer_destroy(nullptr);
}

TEST(CApiTest, ReadsWhatTheCppApiReads)
{
    OpenDocument file(kRgbLevelsPsd);
    const ffpsd::Document cpp = ffpsd::Document::Open(kRgbLevelsPsd);

    EXPECT_EQ(ffpsd_document_get_width(file.doc), cpp.GetWidth());
    EXPECT_EQ(ffpsd_document_get_height(file.doc), cpp.GetHeight());
    EXPECT_EQ(ffpsd_document_get_channel_count(file.doc), 3u);
    EXPECT_EQ(ffpsd_document_get_depth(file.doc), 8u);
    EXPECT_EQ(ffpsd_document_get_color_mode(file.doc), FFPSD_COLOR_MODE_RGB);
    EXPECT_EQ(ffpsd_document_is_psb(file.doc), 0);
    ASSERT_EQ(ffpsd_document_get_layer_count(file.doc), 2u);

    const ffpsd_layer_t* background = Layer(file.doc, 0);
    const ffpsd_layer_t* levels = Layer(file.doc, 1);
    EXPECT_EQ(Name(background), kBackgroundName);
    EXPECT_EQ(Name(levels), kLevelsName);
    EXPECT_EQ(ffpsd_layer_get_kind(background), FFPSD_LAYER_KIND_RASTER);
    EXPECT_EQ(ffpsd_layer_get_kind(levels), FFPSD_LAYER_KIND_ADJUSTMENT);
    EXPECT_EQ(ffpsd_layer_get_adjustment_key(levels), Fourcc("levl"));
    EXPECT_EQ(ffpsd_layer_get_opacity(background), 255u);
    EXPECT_EQ(ffpsd_layer_is_visible(background), 1);
    EXPECT_EQ(ffpsd_layer_get_blend_key(background), Fourcc("norm"));
    EXPECT_EQ(Pixels(background), cpp.GetLayerByIndex(0)->GetPixels().bytes);

    ffpsd_rect_t bounds = {};
    ASSERT_EQ(ffpsd_layer_get_bounds(background, &bounds), FFPSD_STATUS_OK);
    EXPECT_EQ(bounds.right, cpp.GetLayerByIndex(0)->GetBounds().right);

    ffpsd_image_t* merged = nullptr;
    ASSERT_EQ(ffpsd_document_get_merged_image(file.doc, &merged), FFPSD_STATUS_OK);
    EXPECT_EQ(Bytes(merged), cpp.GetMergedImage().bytes);
    ffpsd_image_destroy(merged);

    // "Fon" is 6 bytes of UTF-8; 4 bytes of buffer hold 3 of them and the null.
    char buffer[4] = {'x', 'x', 'x', 'x'};
    size_t length = 0;
    ASSERT_EQ(ffpsd_layer_get_name(background, buffer, sizeof(buffer), &length), FFPSD_STATUS_OK);
    EXPECT_EQ(length, 6u);
    EXPECT_EQ(std::memcmp(buffer, kBackgroundName.data(), 3), 0);
    EXPECT_EQ(buffer[3], '\0');
}

TEST(CApiTest, ResolutionAndVersionInfo)
{
    OpenDocument file(kRgbPsd);

    ffpsd_resolution_info_t resolution = {};
    ASSERT_EQ(ffpsd_document_get_resolution_info(file.doc, &resolution), FFPSD_STATUS_OK);
    EXPECT_DOUBLE_EQ(resolution.horizontal, 0x012BFFFE / 65536.0);
    EXPECT_EQ(resolution.horizontal_unit, 2);

    resolution.horizontal = 72.0;
    ASSERT_EQ(ffpsd_document_set_resolution_info(file.doc, &resolution), FFPSD_STATUS_OK);
    resolution.vertical = 0.0;
    EXPECT_EQ(ffpsd_document_set_resolution_info(file.doc, &resolution), FFPSD_STATUS_INVALID_ARGUMENT);

    ffpsd_version_info_t* info = nullptr;
    ASSERT_EQ(ffpsd_document_get_version_info(file.doc, &info), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_version_info_get_writer_name(info), "Adobe Photoshop");
    EXPECT_STREQ(ffpsd_version_info_get_reader_name(info), "Adobe Photoshop 2026");
    EXPECT_EQ(ffpsd_version_info_get_has_real_merged_data(info), 1);

    ASSERT_EQ(ffpsd_version_info_set_writer_name(info, "ffpsd"), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_set_version_info(file.doc, info), FFPSD_STATUS_OK);
    ffpsd_version_info_destroy(info);

    ASSERT_EQ(ffpsd_document_get_version_info(file.doc, &info), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_version_info_get_writer_name(info), "ffpsd");
    ffpsd_version_info_destroy(info);
}

TEST(CApiTest, ADocumentBuiltInCSavesAndOpensAgain)
{
    ffpsd_document_t* doc = NewRgbDocument();
    const ffpsd::Image image = Pattern(3, 2, 4);
    const ffpsd_image_view_t view = View(image);
    ffpsd_layer_t* layer = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer(doc, "one", &view, 1, 2, &layer), FFPSD_STATUS_OK) << ffpsd_last_error();

    ffpsd_levels_t* levels = nullptr;
    ASSERT_EQ(ffpsd_levels_create(&levels), FFPSD_STATUS_OK);
    ffpsd_levels_channel_t red = FFPSD_LEVELS_CHANNEL_IDENTITY;
    red.input_floor = 40;
    ASSERT_EQ(ffpsd_levels_set_channel(levels, 1, &red), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_add_levels_layer(doc, "levels", levels, &layer), FFPSD_STATUS_OK);
    ffpsd_levels_destroy(levels);

    ffpsd_buffer_t* saved = nullptr;
    ASSERT_EQ(ffpsd_document_save_memory(doc, FFPSD_COMPRESSION_RLE, &saved), FFPSD_STATUS_OK) << ffpsd_last_error();
    ffpsd_document_destroy(doc);

    ffpsd_document_t* back = nullptr;
    ASSERT_EQ(ffpsd_document_open_memory(ffpsd_buffer_get_data(saved), ffpsd_buffer_get_size(saved), &back), FFPSD_STATUS_OK)
        << ffpsd_last_error();
    ffpsd_buffer_destroy(saved);

    ASSERT_EQ(ffpsd_document_get_layer_count(back), 2u);
    EXPECT_EQ(Name(Layer(back, 0)), "one");
    EXPECT_EQ(Pixels(Layer(back, 0)), image.bytes);

    ASSERT_EQ(ffpsd_layer_get_levels(Layer(back, 1), &levels), FFPSD_STATUS_OK);
    ffpsd_levels_channel_t read = {};
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 1, &read), FFPSD_STATUS_OK);
    EXPECT_EQ(read.input_floor, 40u);
    ffpsd_levels_destroy(levels);
    ffpsd_document_destroy(back);

    // Past the end, the records in between are the identity.
    ASSERT_EQ(ffpsd_levels_create(&levels), FFPSD_STATUS_OK);
    const ffpsd_levels_channel_t record = {10, 200, 0, 255, 2.0};
    ASSERT_EQ(ffpsd_levels_set_channel(levels, 2, &record), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_levels_get_count(levels), 3u);
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 0, &read), FFPSD_STATUS_OK);
    EXPECT_EQ(read.input_ceiling, 255u);
    EXPECT_DOUBLE_EQ(read.gamma, 1.0);
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 2, &read), FFPSD_STATUS_OK);
    EXPECT_DOUBLE_EQ(read.gamma, 2.0);
    EXPECT_EQ(ffpsd_levels_get_channel(levels, 3, &read), FFPSD_STATUS_OUT_OF_RANGE);
    ffpsd_levels_destroy(levels);
}

TEST(CApiTest, SaveTakesACompressionAndAPath)
{
    OpenDocument file(kRgbPsd);
    ffpsd_buffer_t* raw = nullptr;
    ffpsd_buffer_t* rle = nullptr;
    ASSERT_EQ(ffpsd_document_save_memory(file.doc, FFPSD_COMPRESSION_RAW, &raw), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_save_memory(file.doc, FFPSD_COMPRESSION_RLE, &rle), FFPSD_STATUS_OK);
    EXPECT_GT(ffpsd_buffer_get_size(raw), ffpsd_buffer_get_size(rle));
    EXPECT_EQ(ffpsd_buffer_get_size(rle), ReadFile(kRgbPsd).size());
    ffpsd_buffer_destroy(raw);
    ffpsd_buffer_destroy(rle);

    const std::string path = testing::TempDir() + "ffpsd_c_api_test.psd";
    ASSERT_EQ(ffpsd_document_save(file.doc, path.c_str(), FFPSD_COMPRESSION_RLE), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ReadFile(path), ReadFile(kRgbPsd));
    std::remove(path.c_str());
}

TEST(CApiTest, StackAndLayerEditsThroughC)
{
    OpenDocument file(kRgbPsd);
    ffpsd_layer_t* copy = nullptr;

    ASSERT_EQ(ffpsd_document_add_layer_copy(file.doc, Layer(file.doc, 1), &copy), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_move_layer(file.doc, 2, 1), FFPSD_STATUS_OK);
    EXPECT_EQ(Layer(file.doc, 1), copy);
    EXPECT_EQ(ffpsd_document_move_layer(file.doc, 1, 0), FFPSD_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ(ffpsd_layer_set_opacity(copy, 10), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_set_visible(copy, 0), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_remove_layer(file.doc, 2), FFPSD_STATUS_OK);

    ASSERT_EQ(ffpsd_document_get_layer_count(file.doc), 2u);
    EXPECT_EQ(ffpsd_layer_get_opacity(copy), 10u);
    EXPECT_EQ(ffpsd_layer_is_visible(copy), 0);
    EXPECT_EQ(ffpsd_document_get_has_real_merged_data(file.doc), 0);

    ffpsd_document_t* doc = NewRgbDocument();
    const ffpsd::Image image = Pattern(3, 2, 4);
    const ffpsd_image_view_t layer_view = View(image);
    ffpsd_layer_t* layer = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer(doc, "layer", &layer_view, 0, 0, &layer), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_set_position(layer, 5, 6), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_resize(layer, 6, 4, FFPSD_RESAMPLE_FILTER_BICUBIC), FFPSD_STATUS_OK);

    ffpsd_rect_t bounds = {};
    ASSERT_EQ(ffpsd_layer_get_bounds(layer, &bounds), FFPSD_STATUS_OK);
    EXPECT_EQ(bounds.top, 5);
    EXPECT_EQ(bounds.left, 6);
    EXPECT_EQ(bounds.right - bounds.left, 6);
    EXPECT_EQ(ffpsd_layer_resize(layer, 0, 4, FFPSD_RESAMPLE_FILTER_NEAREST), FFPSD_STATUS_INVALID_ARGUMENT);
    ffpsd_document_destroy(doc);
}

TEST(CApiTest, ABackgroundThroughC)
{
    ffpsd_document_t* doc = NewRgbDocument();
    const ffpsd::Image image = Pattern(4, 3, 3);
    const ffpsd_image_view_t view = View(image);
    ffpsd_layer_t* top = nullptr;
    ffpsd_layer_t* background = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer(doc, "top", nullptr, 0, 0, &top), FFPSD_STATUS_OK);

    ASSERT_EQ(ffpsd_document_add_background_layer(doc, "Background", &view, &background), FFPSD_STATUS_OK) << ffpsd_last_error();

    EXPECT_EQ(Layer(doc, 0), background);
    EXPECT_EQ(ffpsd_layer_is_background(background), 1);
    EXPECT_EQ(ffpsd_layer_is_background(top), 0);
    EXPECT_EQ(ffpsd_document_add_background_layer(doc, "again", &view, &background), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(background, nullptr);

    EXPECT_EQ(ffpsd_document_unset_background_layer(doc), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_is_background(Layer(doc, 0)), 0);
    EXPECT_EQ(ffpsd_document_unset_background_layer(doc), FFPSD_STATUS_NOT_FOUND);

    ASSERT_EQ(ffpsd_document_set_background_layer(doc, 1), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(Layer(doc, 0), top);
    EXPECT_EQ(ffpsd_layer_is_background(top), 1);
    EXPECT_EQ(ffpsd_document_set_background_layer(doc, 1), FFPSD_STATUS_INVALID_OPERATION);
    ffpsd_document_destroy(doc);
}

TEST(CApiTest, ColorModeConvertsThroughC)
{
    ffpsd_document_t* doc = NewRgbDocument();
    const ffpsd::Image image = Pattern(3, 2, 4);
    const ffpsd_image_view_t view = View(image);
    ffpsd_layer_t* layer = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer(doc, "one", &view, 0, 0, &layer), FFPSD_STATUS_OK);

    EXPECT_EQ(ffpsd_document_convert_color_mode(doc, FFPSD_COLOR_MODE_CMYK), FFPSD_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ(ffpsd_document_convert_color_mode(doc, FFPSD_COLOR_MODE_GRAYSCALE), FFPSD_STATUS_OK) << ffpsd_last_error();

    EXPECT_EQ(ffpsd_document_get_color_mode(doc), FFPSD_COLOR_MODE_GRAYSCALE);
    EXPECT_EQ(ffpsd_document_get_channel_count(doc), 1u);
    EXPECT_EQ(Pixels(layer).size(), std::size_t{3} * 2 * 2);
    ffpsd_document_destroy(doc);
}

TEST(CApiTest, ResourcesAndBlocksAreBorrowed)
{
    OpenDocument file(kRgbPsd);

    ffpsd_image_resource_t seed = {};
    ASSERT_EQ(ffpsd_document_get_image_resource_by_id(file.doc, 1044, &seed), FFPSD_STATUS_OK);
    EXPECT_EQ(BigEndianU32(std::vector<std::uint8_t>(seed.data, seed.data + seed.size)), 2u);

    const std::uint8_t bytes[] = {1, 2, 3};
    const ffpsd_image_resource_t resource = {4000, nullptr, bytes, sizeof(bytes)};
    ASSERT_EQ(ffpsd_document_set_image_resource(file.doc, &resource), FFPSD_STATUS_OK);
    ffpsd_image_resource_t read = {};
    ASSERT_EQ(ffpsd_document_get_image_resource_by_id(file.doc, 4000, &read), FFPSD_STATUS_OK);
    EXPECT_STREQ(read.name, "");
    EXPECT_EQ(std::vector<std::uint8_t>(read.data, read.data + read.size), (std::vector<std::uint8_t>{1, 2, 3}));
    EXPECT_EQ(ffpsd_document_remove_image_resource(file.doc, 4000), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_document_remove_image_resource(file.doc, 4000), FFPSD_STATUS_NOT_FOUND);

    ffpsd_layer_t* layer = Layer(file.doc, 0);
    const ffpsd_tagged_block_t block = {Fourcc("8BIM"), Fourcc("abcd"), bytes, sizeof(bytes)};
    ASSERT_EQ(ffpsd_layer_set_tagged_block(layer, &block), FFPSD_STATUS_OK);
    ffpsd_tagged_block_t found = {};
    ASSERT_EQ(ffpsd_layer_get_tagged_block_by_key(layer, Fourcc("abcd"), &found), FFPSD_STATUS_OK);
    EXPECT_EQ(found.size, 3u);
    EXPECT_EQ(ffpsd_layer_remove_tagged_block(layer, Fourcc("abcd")), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_get_tagged_block_by_key(layer, Fourcc("abcd"), &found), FFPSD_STATUS_NOT_FOUND);

    ffpsd_tagged_block_t first = {};
    ASSERT_EQ(ffpsd_document_get_tagged_block_by_index(file.doc, 0, &first), FFPSD_STATUS_OK);
    EXPECT_EQ(first.key, Fourcc("Patt"));
}

TEST(CApiTest, EveryFailureIsAStatusWithAMessage)
{
    OpenDocument file(kRgbPsd);
    ffpsd_document_t* doc = nullptr;
    ffpsd_layer_t* layer = Layer(file.doc, 0);
    ffpsd_image_resource_t resource = {};
    const std::uint8_t garbage[16] = {};

    EXPECT_EQ(ffpsd_document_open(DataFile("no_such_file.psd").c_str(), &doc), FFPSD_STATUS_IO);
    EXPECT_STRNE(ffpsd_last_error(), "");
    EXPECT_EQ(ffpsd_document_open_memory(garbage, sizeof(garbage), &doc), FFPSD_STATUS_INVALID_FILE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(ffpsd_document_open(nullptr, &doc), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_open(kRgbPsd.c_str(), nullptr), FFPSD_STATUS_INVALID_ARGUMENT);

    // A failed get clears the out pointer.
    EXPECT_EQ(ffpsd_document_get_layer(file.doc, 5, &layer), FFPSD_STATUS_OUT_OF_RANGE);
    EXPECT_EQ(layer, nullptr);
    EXPECT_EQ(ffpsd_document_get_image_resource_by_id(file.doc, 9999, &resource), FFPSD_STATUS_NOT_FOUND);
    EXPECT_EQ(ffpsd_document_get_image_resource_by_id(file.doc, 9999, nullptr), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_set_background_layer(file.doc, 1), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_document_set_width(file.doc, 0), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_set_width(file.doc, 100), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_document_set_depth(file.doc, 16), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_document_create_with(4, 3, FFPSD_COLOR_MODE_MULTICHANNEL, 8, &doc), FFPSD_STATUS_INVALID_ARGUMENT);

    // An empty document has no size to save.
    ffpsd_buffer_t* saved = nullptr;
    ASSERT_EQ(ffpsd_document_create(&doc), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_document_save_memory(doc, FFPSD_COMPRESSION_RLE, &saved), FFPSD_STATUS_INVALID_OPERATION);
    ffpsd_document_destroy(doc);
    EXPECT_EQ(ffpsd_layer_set_levels(Layer(file.doc, 0), nullptr), FFPSD_STATUS_INVALID_ARGUMENT);

    // A call that succeeds leaves no message behind.
    EXPECT_EQ(ffpsd_layer_set_opacity(Layer(file.doc, 1), 128), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_last_error(), "");
}

TEST(CApiTest, PngAndJpegThroughC)
{
    OpenDocument file(kRgbPsd);
    const ffpsd_layer_t* layer = Layer(file.doc, 1);
    ffpsd_buffer_t* png = nullptr;
    const ffpsd_status_t status = ffpsd_layer_save_as_png_memory(layer, &png);

#if defined(FFPSD_HAS_PNG)
    ASSERT_EQ(status, FFPSD_STATUS_OK) << ffpsd_last_error();
    ffpsd_image_t* image = nullptr;
    ASSERT_EQ(
        ffpsd_png_load_memory(ffpsd_buffer_get_data(png), ffpsd_buffer_get_size(png), FFPSD_COLOR_MODE_RGB, 8, &image), FFPSD_STATUS_OK);
    EXPECT_EQ(Bytes(image), Pixels(layer));
    ffpsd_image_destroy(image);
    ffpsd_buffer_destroy(png);
#else
    EXPECT_EQ(status, FFPSD_STATUS_UNSUPPORTED);
    EXPECT_EQ(png, nullptr);
#endif

    ffpsd_buffer_t* jpeg = nullptr;
    const ffpsd_status_t jpeg_status = ffpsd_layer_save_as_jpeg_memory(layer, 90, &jpeg);

#if defined(FFPSD_HAS_JPEG)
    ASSERT_EQ(jpeg_status, FFPSD_STATUS_OK) << ffpsd_last_error();
    ffpsd_image_t* decoded = nullptr;
    ASSERT_EQ(
        ffpsd_jpeg_load_memory(ffpsd_buffer_get_data(jpeg), ffpsd_buffer_get_size(jpeg), FFPSD_COLOR_MODE_RGB, 8, 1, &decoded),
        FFPSD_STATUS_OK)
        << ffpsd_last_error();
    ffpsd_image_view_t view = {};
    ASSERT_EQ(ffpsd_image_get_view(decoded, &view), FFPSD_STATUS_OK);
    EXPECT_EQ(view.channel_count, 3u);
    EXPECT_EQ(view.size, Pixels(layer).size() / 4 * 3); // the layer's transparency is dropped
    ffpsd_image_destroy(decoded);
    ffpsd_buffer_destroy(jpeg);
    EXPECT_EQ(ffpsd_layer_save_as_jpeg_memory(layer, 0, &jpeg), FFPSD_STATUS_INVALID_ARGUMENT);
#else
    EXPECT_EQ(jpeg_status, FFPSD_STATUS_UNSUPPORTED);
    EXPECT_EQ(jpeg, nullptr);
#endif
}
