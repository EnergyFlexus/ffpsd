// The C API alone, as a C program sees it.
#include "support/test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ffpsd/c_api.h>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using namespace ffpsd_test;

extern "C" int ffpsd_test_count_layers_from_c(const char* path);

namespace
{
    void Destroy(ffpsd_document_t* handle)
    {
        ffpsd_document_destroy(handle);
    }
    void Destroy(ffpsd_image_t* handle)
    {
        ffpsd_image_destroy(handle);
    }
    void Destroy(ffpsd_buffer_t* handle)
    {
        ffpsd_buffer_destroy(handle);
    }
    void Destroy(ffpsd_levels_t* handle)
    {
        ffpsd_levels_destroy(handle);
    }
    void Destroy(ffpsd_version_info_t* handle)
    {
        ffpsd_version_info_destroy(handle);
    }

    // Destroys the handle a call put into it, the old one before out hands it to the next call.
    template <class T> class Owned
    {
    public:
        Owned() = default;
        Owned(Owned&& other) noexcept
            : handle_(std::exchange(other.handle_, nullptr))
        {
        }
        Owned& operator=(Owned&&) = delete;
        ~Owned()
        {
            Destroy(handle_);
        }

        operator T*() const noexcept
        {
            return handle_;
        }
        T** out() noexcept
        {
            Destroy(std::exchange(handle_, nullptr));
            return &handle_;
        }

    private:
        T* handle_ = nullptr;
    };

    Owned<ffpsd_document_t> Open(const std::string& path)
    {
        Owned<ffpsd_document_t> doc;
        EXPECT_EQ(ffpsd_document_open(path.c_str(), doc.out()), FFPSD_STATUS_OK) << ffpsd_last_error();
        return doc;
    }

    Owned<ffpsd_document_t> NewRgbDocument()
    {
        Owned<ffpsd_document_t> doc;
        EXPECT_EQ(ffpsd_document_create_with(4, 3, FFPSD_COLOR_MODE_RGB, 8, doc.out()), FFPSD_STATUS_OK);
        return doc;
    }

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

    ffpsd_image_view_t ViewOf(const ffpsd_image_t* image)
    {
        ffpsd_image_view_t view = {};
        EXPECT_EQ(ffpsd_image_get_view(image, &view), FFPSD_STATUS_OK);
        return view;
    }

    ffpsd::Bytes Bytes(const ffpsd_image_t* image)
    {
        const ffpsd_image_view_t view = ViewOf(image);
        return ffpsd::Bytes(view.data, view.size);
    }

    ffpsd::Bytes Pixels(const ffpsd_layer_t* layer)
    {
        Owned<ffpsd_image_t> image;
        EXPECT_EQ(ffpsd_layer_get_pixels(layer, image.out()), FFPSD_STATUS_OK) << ffpsd_last_error();
        return Bytes(image);
    }

    std::vector<std::uint8_t> Data(const std::uint8_t* data, size_t size)
    {
        return std::vector<std::uint8_t>(data, data + size);
    }

    ffpsd_image_view_t View(const ffpsd::Image& image)
    {
        return {
            image.width,        image.height,       image.channel_count, image.depth, static_cast<ffpsd_color_mode_t>(image.color_mode),
            image.bytes.data(), image.bytes.size(),
        };
    }

    ffpsd_layer_t* AddLayer(ffpsd_document_t* doc, const char* name, const ffpsd::Image& image, int32_t top = 0, int32_t left = 0)
    {
        const ffpsd_image_view_t view = View(image);
        ffpsd_layer_t* layer = nullptr;
        EXPECT_EQ(ffpsd_document_add_layer(doc, name, &view, top, left, &layer), FFPSD_STATUS_OK) << ffpsd_last_error();
        return layer;
    }
} // namespace

TEST(CApiTest, FailuresAreStatusesWithAMessageAndNullHandlesAreHarmless)
{
    EXPECT_EQ(ffpsd_test_count_layers_from_c(kRgbPsd.c_str()), 2);

    const std::string version = ffpsd_version();
    EXPECT_EQ(std::count(version.begin(), version.end(), '.'), 2);

    EXPECT_EQ(ffpsd_document_get_width(nullptr), 0u);
    EXPECT_EQ(ffpsd_document_get_layer_count(nullptr), 0u);
    EXPECT_EQ(ffpsd_layer_get_opacity(nullptr), 0u);
    EXPECT_EQ(ffpsd_layer_get_channel_count(nullptr), 0u);
    EXPECT_EQ(ffpsd_buffer_get_data(nullptr), nullptr);
    EXPECT_EQ(ffpsd_version_info_get_writer_name(nullptr), nullptr);
    ffpsd_document_destroy(nullptr);
    ffpsd_image_destroy(nullptr);
    ffpsd_buffer_destroy(nullptr);

    const Owned<ffpsd_document_t> file = Open(kRgbPsd);
    ffpsd_document_t* doc = nullptr;
    ffpsd_layer_t* layer = Layer(file, 0);
    ffpsd_image_resource_t resource = {};
    const std::uint8_t garbage[16] = {};

    EXPECT_EQ(ffpsd_document_open(DataFile("no_such_file.psd").c_str(), &doc), FFPSD_STATUS_IO);
    EXPECT_STRNE(ffpsd_last_error(), "");
    EXPECT_EQ(ffpsd_document_open_memory(garbage, sizeof(garbage), &doc), FFPSD_STATUS_INVALID_FILE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(ffpsd_document_open(nullptr, &doc), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_open(kRgbPsd.c_str(), nullptr), FFPSD_STATUS_INVALID_ARGUMENT);

    // A failed get clears the out pointer.
    EXPECT_EQ(ffpsd_document_get_layer(file, 5, &layer), FFPSD_STATUS_OUT_OF_RANGE);
    EXPECT_EQ(layer, nullptr);
    EXPECT_EQ(ffpsd_document_get_image_resource_by_id(file, 9999, &resource), FFPSD_STATUS_NOT_FOUND);
    EXPECT_EQ(ffpsd_document_get_image_resource_by_id(file, 9999, nullptr), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_set_background_layer(file, 1), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_document_resize_canvas(file, 0, 100, FFPSD_ANCHOR_CENTER), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_resize(nullptr, 100, 100, FFPSD_RESAMPLE_FILTER_BICUBIC), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_document_create_with(4, 3, FFPSD_COLOR_MODE_MULTICHANNEL, 8, &doc), FFPSD_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(ffpsd_layer_set_levels(Layer(file, 0), nullptr), FFPSD_STATUS_INVALID_ARGUMENT);

    // A call that succeeds leaves no message behind.
    EXPECT_EQ(ffpsd_layer_set_opacity(Layer(file, 1), 128), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_last_error(), "");
}

TEST(CApiTest, ReadsWhatTheCppApiReads)
{
    const Owned<ffpsd_document_t> file = Open(kRgbLevelsPsd);
    const ffpsd::Document cpp = ffpsd::Document::Open(kRgbLevelsPsd);

    EXPECT_EQ(ffpsd_document_get_width(file), cpp.GetWidth());
    EXPECT_EQ(ffpsd_document_get_height(file), cpp.GetHeight());
    EXPECT_EQ(ffpsd_document_get_channel_count(file), 3u);
    EXPECT_EQ(ffpsd_document_get_depth(file), 8u);
    EXPECT_EQ(ffpsd_document_get_color_mode(file), FFPSD_COLOR_MODE_RGB);
    EXPECT_EQ(ffpsd_document_is_psb(file), 0);
    ASSERT_EQ(ffpsd_document_get_layer_count(file), 2u);

    const ffpsd_layer_t* background = Layer(file, 0);
    const ffpsd_layer_t* levels = Layer(file, 1);
    EXPECT_EQ(Name(background), kBackgroundName);
    EXPECT_EQ(Name(levels), kLevelsName);
    EXPECT_EQ(ffpsd_layer_get_kind(background), FFPSD_LAYER_KIND_RASTER);
    EXPECT_EQ(ffpsd_layer_get_kind(levels), FFPSD_LAYER_KIND_ADJUSTMENT);
    EXPECT_EQ(ffpsd_layer_get_adjustment_key(levels), Fourcc("levl"));
    EXPECT_EQ(ffpsd_layer_get_opacity(background), 255u);
    EXPECT_EQ(ffpsd_layer_is_visible(background), 1);
    EXPECT_EQ(ffpsd_layer_get_blend_key(background), Fourcc("norm"));
    EXPECT_EQ(Pixels(background), Pixels(cpp, 0));

    ffpsd_rect_t bounds = {};
    ASSERT_EQ(ffpsd_layer_get_bounds(background, &bounds), FFPSD_STATUS_OK);
    EXPECT_EQ(bounds.right, cpp.GetLayerByIndex(0)->GetBounds().right);

    Owned<ffpsd_image_t> merged;
    ASSERT_EQ(ffpsd_document_get_merged_image(file, merged.out()), FFPSD_STATUS_OK);
    EXPECT_EQ(Bytes(merged), cpp.GetMergedImage().bytes);

    // "Fon" is 6 bytes of UTF-8; 4 bytes of buffer hold 3 of them and the null.
    char buffer[4] = {'x', 'x', 'x', 'x'};
    size_t length = 0;
    ASSERT_EQ(ffpsd_layer_get_name(background, buffer, sizeof(buffer), &length), FFPSD_STATUS_OK);
    EXPECT_EQ(length, 6u);
    EXPECT_EQ(std::memcmp(buffer, kBackgroundName.data(), 3), 0);
    EXPECT_EQ(buffer[3], '\0');

    // Pixels into the caller's memory, exactly the size the info gives.
    ffpsd_image_view_t info = {};
    ASSERT_EQ(ffpsd_layer_get_pixels_info(background, &info), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(info.data, nullptr);
    EXPECT_EQ(info.size, std::size_t{info.width} * info.height * info.channel_count * (info.depth / 8));
    ffpsd::Bytes bytes(info.size);
    ASSERT_EQ(ffpsd_layer_get_pixels_bytes(background, bytes.data(), bytes.size()), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(bytes, Pixels(background));
    EXPECT_EQ(ffpsd_layer_get_pixels_bytes(background, bytes.data(), bytes.size() - 1), FFPSD_STATUS_INVALID_ARGUMENT);

    ffpsd_image_view_t merged_info = {};
    ASSERT_EQ(ffpsd_document_get_merged_image_info(file, &merged_info), FFPSD_STATUS_OK) << ffpsd_last_error();
    ffpsd::Bytes merged_bytes(merged_info.size);
    ASSERT_EQ(ffpsd_document_get_merged_image_bytes(file, merged_bytes.data(), merged_bytes.size()), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(merged_bytes, Bytes(merged));

    // Channels tell how they are stored.
    const Owned<ffpsd_document_t> rgb = Open(kRgbPsd);
    ffpsd_channel_compression_t merged_compression = FFPSD_CHANNEL_COMPRESSION_RAW;
    ASSERT_EQ(ffpsd_document_get_merged_compression(rgb, &merged_compression), FFPSD_STATUS_OK);
    EXPECT_EQ(merged_compression, FFPSD_CHANNEL_COMPRESSION_RLE);
    const ffpsd_layer_t* copy = Layer(rgb, 1);
    ASSERT_EQ(ffpsd_layer_get_channel_count(copy), 4u);
    ffpsd_channel_info_t channel{};
    ASSERT_EQ(ffpsd_layer_get_channel_by_index(copy, 0, &channel), FFPSD_STATUS_OK);
    EXPECT_EQ(channel.compression, FFPSD_CHANNEL_COMPRESSION_RLE);
    EXPECT_GT(channel.size, 2u);
    EXPECT_EQ(ffpsd_layer_get_channel_by_index(copy, 4, &channel), FFPSD_STATUS_OUT_OF_RANGE);
}

TEST(CApiTest, ResourcesAndBlocksAreBorrowed)
{
    const Owned<ffpsd_document_t> file = Open(kRgbPsd);

    ffpsd_image_resource_t seed = {};
    ASSERT_EQ(ffpsd_document_get_image_resource_by_id(file, 1044, &seed), FFPSD_STATUS_OK);
    EXPECT_EQ(BigEndianU32(Data(seed.data, seed.size)), 2u);

    const std::uint8_t bytes[] = {1, 2, 3};
    const ffpsd_image_resource_t resource = {4000, nullptr, bytes, sizeof(bytes)};
    ASSERT_EQ(ffpsd_document_set_image_resource(file, &resource), FFPSD_STATUS_OK);
    ffpsd_image_resource_t read = {};
    ASSERT_EQ(ffpsd_document_get_image_resource_by_id(file, 4000, &read), FFPSD_STATUS_OK);
    EXPECT_STREQ(read.name, "");
    EXPECT_EQ(Data(read.data, read.size), (std::vector<std::uint8_t>{1, 2, 3}));
    EXPECT_EQ(ffpsd_document_remove_image_resource(file, 4000), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_document_remove_image_resource(file, 4000), FFPSD_STATUS_NOT_FOUND);

    ffpsd_layer_t* layer = Layer(file, 0);
    const ffpsd_tagged_block_t block = {Fourcc("8BIM"), Fourcc("abcd"), bytes, sizeof(bytes)};
    ASSERT_EQ(ffpsd_layer_set_tagged_block(layer, &block), FFPSD_STATUS_OK);
    ffpsd_tagged_block_t found = {};
    ASSERT_EQ(ffpsd_layer_get_tagged_block_by_key(layer, Fourcc("abcd"), &found), FFPSD_STATUS_OK);
    EXPECT_EQ(found.size, 3u);
    EXPECT_EQ(ffpsd_layer_remove_tagged_block(layer, Fourcc("abcd")), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_get_tagged_block_by_key(layer, Fourcc("abcd"), &found), FFPSD_STATUS_NOT_FOUND);

    ffpsd_tagged_block_t first = {};
    ASSERT_EQ(ffpsd_document_get_tagged_block_by_index(file, 0, &first), FFPSD_STATUS_OK);
    EXPECT_EQ(first.key, Fourcc("Patt"));

    // Resolution and version info have structs of their own.
    ffpsd_resolution_info_t resolution = {};
    ASSERT_EQ(ffpsd_document_get_resolution_info(file, &resolution), FFPSD_STATUS_OK);
    EXPECT_DOUBLE_EQ(resolution.horizontal, 0x012BFFFE / 65536.0);
    EXPECT_EQ(resolution.horizontal_unit, 2);
    resolution.horizontal = 72.0;
    ASSERT_EQ(ffpsd_document_set_resolution_info(file, &resolution), FFPSD_STATUS_OK);
    resolution.vertical = 0.0;
    EXPECT_EQ(ffpsd_document_set_resolution_info(file, &resolution), FFPSD_STATUS_INVALID_ARGUMENT);

    Owned<ffpsd_version_info_t> info;
    ASSERT_EQ(ffpsd_document_get_version_info(file, info.out()), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_version_info_get_writer_name(info), "Adobe Photoshop");
    EXPECT_STREQ(ffpsd_version_info_get_reader_name(info), "Adobe Photoshop 2026");
    EXPECT_EQ(ffpsd_version_info_get_has_real_merged_data(info), 1);
    ASSERT_EQ(ffpsd_version_info_set_writer_name(info, "ffpsd"), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_set_version_info(file, info), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_get_version_info(file, info.out()), FFPSD_STATUS_OK);
    EXPECT_STREQ(ffpsd_version_info_get_writer_name(info), "ffpsd");
}

TEST(CApiTest, SavesToMemoryOrAPathAndOpensAgain)
{
    const Owned<ffpsd_document_t> doc = NewRgbDocument();
    const ffpsd::Image image = Pattern(3, 2, 4);
    ffpsd_image_view_t lab = View(image);
    lab.color_mode = FFPSD_COLOR_MODE_LAB;
    ffpsd_layer_t* layer = nullptr;
    EXPECT_EQ(ffpsd_document_add_layer(doc, "lab", &lab, 0, 0, &layer), FFPSD_STATUS_INVALID_ARGUMENT);
    AddLayer(doc, "one", image, 1, 2);

    Owned<ffpsd_levels_t> levels;
    ASSERT_EQ(ffpsd_levels_create(levels.out()), FFPSD_STATUS_OK);
    ffpsd_levels_channel_t red = FFPSD_LEVELS_CHANNEL_IDENTITY;
    red.input_floor = 40;
    ASSERT_EQ(ffpsd_levels_set_channel(levels, 1, &red), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_add_levels_layer(doc, "levels", levels, &layer), FFPSD_STATUS_OK);

    Owned<ffpsd_buffer_t> saved;
    ASSERT_EQ(ffpsd_document_save_memory(doc, FFPSD_COMPRESSION_RLE, saved.out()), FFPSD_STATUS_OK) << ffpsd_last_error();
    Owned<ffpsd_document_t> back;
    ASSERT_EQ(ffpsd_document_open_memory(ffpsd_buffer_get_data(saved), ffpsd_buffer_get_size(saved), back.out()), FFPSD_STATUS_OK)
        << ffpsd_last_error();

    ASSERT_EQ(ffpsd_document_get_layer_count(back), 2u);
    EXPECT_EQ(Name(Layer(back, 0)), "one");
    EXPECT_EQ(Pixels(Layer(back, 0)), image.bytes);
    ASSERT_EQ(ffpsd_layer_get_levels(Layer(back, 1), levels.out()), FFPSD_STATUS_OK);
    ffpsd_levels_channel_t read = {};
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 1, &read), FFPSD_STATUS_OK);
    EXPECT_EQ(read.input_floor, 40u);

    // Past the end, the records in between are the identity.
    ASSERT_EQ(ffpsd_levels_create(levels.out()), FFPSD_STATUS_OK);
    const ffpsd_levels_channel_t record = {10, 200, 0, 255, 2.0};
    ASSERT_EQ(ffpsd_levels_set_channel(levels, 2, &record), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_levels_get_count(levels), 3u);
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 0, &read), FFPSD_STATUS_OK);
    EXPECT_EQ(read.input_ceiling, 255u);
    EXPECT_DOUBLE_EQ(read.gamma, 1.0);
    ASSERT_EQ(ffpsd_levels_get_channel(levels, 2, &read), FFPSD_STATUS_OK);
    EXPECT_DOUBLE_EQ(read.gamma, 2.0);
    EXPECT_EQ(ffpsd_levels_get_channel(levels, 3, &read), FFPSD_STATUS_OUT_OF_RANGE);

    // Photoshop's file in each compression, to memory and to a path.
    const Owned<ffpsd_document_t> file = Open(kRgbPsd);
    Owned<ffpsd_buffer_t> raw;
    Owned<ffpsd_buffer_t> rle;
    ASSERT_EQ(ffpsd_document_save_memory(file, FFPSD_COMPRESSION_RAW, raw.out()), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_save_memory(file, FFPSD_COMPRESSION_RLE, rle.out()), FFPSD_STATUS_OK);
    EXPECT_GT(ffpsd_buffer_get_size(raw), ffpsd_buffer_get_size(rle));
    EXPECT_EQ(ffpsd_buffer_get_size(rle), ReadFile(kRgbPsd).size());

    const TempFile path("ffpsd_c_api_test.psd");
    for (const ffpsd_compression_t compression : {FFPSD_COMPRESSION_RLE, FFPSD_COMPRESSION_DEFAULT})
    {
        ASSERT_EQ(ffpsd_document_save(file, path.Utf8().c_str(), compression), FFPSD_STATUS_OK) << ffpsd_last_error();
        EXPECT_EQ(ReadFile(path.Path()), ReadFile(kRgbPsd));
    }
}

TEST(CApiTest, EditsTheStackAndItsLayers)
{
    const Owned<ffpsd_document_t> file = Open(kRgbPsd);
    ffpsd_layer_t* copy = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer_copy(file, Layer(file, 1), &copy), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_move_layer(file, 2, 1), FFPSD_STATUS_OK);
    EXPECT_EQ(Layer(file, 1), copy);
    EXPECT_EQ(ffpsd_document_move_layer(file, 1, 0), FFPSD_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ(ffpsd_layer_set_name(copy, "copy"), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_set_name(copy, nullptr), FFPSD_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ(ffpsd_layer_set_opacity(copy, 10), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_set_visible(copy, 0), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_set_clipped(copy, 1), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_remove_layer(file, 2), FFPSD_STATUS_OK);

    ASSERT_EQ(ffpsd_document_get_layer_count(file), 2u);
    EXPECT_EQ(Name(copy), "copy");
    EXPECT_EQ(ffpsd_layer_get_opacity(copy), 10u);
    EXPECT_EQ(ffpsd_layer_is_visible(copy), 0);
    EXPECT_EQ(ffpsd_layer_is_clipped(copy), 1);
    EXPECT_EQ(ffpsd_document_get_has_real_merged_data(file), 0);

    const Owned<ffpsd_document_t> doc = NewRgbDocument();
    ffpsd_layer_t* layer = AddLayer(doc, "layer", Pattern(3, 2, 4));
    ASSERT_EQ(ffpsd_layer_set_position(layer, 5, 6), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_layer_resize(layer, 6, 4, FFPSD_RESAMPLE_FILTER_BICUBIC), FFPSD_STATUS_OK);
    ffpsd_rect_t bounds = {};
    ASSERT_EQ(ffpsd_layer_get_bounds(layer, &bounds), FFPSD_STATUS_OK);
    EXPECT_EQ(bounds.top, 5);
    EXPECT_EQ(bounds.left, 6);
    EXPECT_EQ(bounds.right - bounds.left, 6);
    EXPECT_EQ(ffpsd_layer_resize(layer, 0, 4, FFPSD_RESAMPLE_FILTER_NEAREST), FFPSD_STATUS_INVALID_ARGUMENT);

    // A group goes on top; the layer from below lands inside, between the end marker and the header.
    ffpsd_layer_t* group = nullptr;
    ASSERT_EQ(ffpsd_document_add_group(doc, "group", &group), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_get_kind(group), FFPSD_LAYER_KIND_GROUP_OPEN);
    EXPECT_EQ(ffpsd_layer_get_kind(Layer(doc, 1)), FFPSD_LAYER_KIND_GROUP_END);
    ASSERT_EQ(ffpsd_document_move_layer(doc, 0, 1), FFPSD_STATUS_OK);
    EXPECT_EQ(Layer(doc, 1), layer);
    EXPECT_EQ(Layer(doc, 2), group);

    std::size_t index = 0;
    ASSERT_EQ(ffpsd_document_find_layer_index(doc, group, &index), FFPSD_STATUS_OK);
    EXPECT_EQ(index, 2u);
    EXPECT_EQ(ffpsd_document_find_layer_index(doc, copy, &index), FFPSD_STATUS_NOT_FOUND);
    ASSERT_EQ(ffpsd_layer_set_group_open(group, 0), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_get_kind(group), FFPSD_LAYER_KIND_GROUP_CLOSED);
    EXPECT_EQ(ffpsd_layer_set_group_open(layer, 1), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_document_add_group(doc, nullptr, &group), FFPSD_STATUS_INVALID_ARGUMENT);

    // A background goes under the rest, once; unset, any raster layer can become it.
    const Owned<ffpsd_document_t> stack = NewRgbDocument();
    const ffpsd::Image picture = Pattern(4, 3, 3);
    const ffpsd_image_view_t view = View(picture);
    ffpsd_layer_t* top = nullptr;
    ffpsd_layer_t* background = nullptr;
    ASSERT_EQ(ffpsd_document_add_layer(stack, "top", nullptr, 0, 0, &top), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_add_background_layer(stack, "Background", &view, &background), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(Layer(stack, 0), background);
    EXPECT_EQ(ffpsd_layer_is_background(background), 1);
    EXPECT_EQ(ffpsd_layer_is_background(top), 0);
    EXPECT_EQ(ffpsd_document_add_background_layer(stack, "again", &view, &background), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(background, nullptr);

    EXPECT_EQ(ffpsd_document_unset_background_layer(stack), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_is_background(Layer(stack, 0)), 0);
    EXPECT_EQ(ffpsd_document_unset_background_layer(stack), FFPSD_STATUS_NOT_FOUND);
    ASSERT_EQ(ffpsd_document_set_background_layer(stack, 1), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(Layer(stack, 0), top);
    EXPECT_EQ(ffpsd_layer_is_background(top), 1);
    EXPECT_EQ(ffpsd_document_set_background_layer(stack, 1), FFPSD_STATUS_INVALID_OPERATION);
}

TEST(CApiTest, MasksTransformsAndColorModes)
{
    const Owned<ffpsd_document_t> file = Open(kRgbMasksPsd);
    Owned<ffpsd_image_t> image;
    ffpsd_rect_t bounds = {};
    std::uint8_t color = 0;
    ASSERT_EQ(ffpsd_layer_get_mask(Layer(file, 2), image.out(), &bounds, &color), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(bounds.left, 935);
    EXPECT_EQ(color, 255);
    EXPECT_EQ(ffpsd_layer_get_mask(Layer(file, 1), image.out(), &bounds, &color), FFPSD_STATUS_NOT_FOUND);
    EXPECT_EQ(image, nullptr);

    const ffpsd::Image mask = Pattern(2, 2, 1);
    const ffpsd_image_view_t view = View(mask);
    EXPECT_EQ(ffpsd_layer_set_mask(Layer(file, 1), &view, 5, 6, 0), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ffpsd_layer_remove_mask(Layer(file, 1)), FFPSD_STATUS_OK);
    EXPECT_EQ(ffpsd_layer_remove_mask(Layer(file, 1)), FFPSD_STATUS_NOT_FOUND);
    EXPECT_EQ(ffpsd_layer_set_mask(Layer(file, 3), &view, 0, 0, 255), FFPSD_STATUS_INVALID_OPERATION);

    EXPECT_EQ(ffpsd_document_resize_canvas(file, 2000, 1500, FFPSD_ANCHOR_CENTER), FFPSD_STATUS_INVALID_OPERATION);
    ASSERT_EQ(ffpsd_document_resize(file, 945, 709, FFPSD_RESAMPLE_FILTER_BICUBIC), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ffpsd_document_get_width(file), 945u);

    EXPECT_EQ(ffpsd_document_rotate_canvas(file, FFPSD_ROTATION_90), FFPSD_STATUS_INVALID_OPERATION);
    EXPECT_EQ(ffpsd_layer_rotate(Layer(file, 1), FFPSD_ROTATION_180), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ffpsd_layer_flip(Layer(file, 0), FFPSD_FLIP_VERTICAL), FFPSD_STATUS_INVALID_OPERATION);
    ASSERT_EQ(ffpsd_document_remove_layer(file, 3), FFPSD_STATUS_OK);
    ASSERT_EQ(ffpsd_document_rotate_canvas(file, FFPSD_ROTATION_90), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ffpsd_document_get_width(file), 709u);
    EXPECT_EQ(ffpsd_document_flip_canvas(file, FFPSD_FLIP_HORIZONTAL), FFPSD_STATUS_OK);

    const Owned<ffpsd_document_t> doc = NewRgbDocument();
    const ffpsd_layer_t* layer = AddLayer(doc, "one", Pattern(3, 2, 4));
    EXPECT_EQ(ffpsd_document_convert_color_mode(doc, FFPSD_COLOR_MODE_CMYK), FFPSD_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ(ffpsd_document_convert_color_mode(doc, FFPSD_COLOR_MODE_GRAYSCALE), FFPSD_STATUS_OK) << ffpsd_last_error();
    EXPECT_EQ(ffpsd_document_get_color_mode(doc), FFPSD_COLOR_MODE_GRAYSCALE);
    EXPECT_EQ(ffpsd_document_get_channel_count(doc), 1u);
    EXPECT_EQ(Pixels(layer).size(), std::size_t{3} * 2 * 2);
}

TEST(CApiTest, PictureFormatsAreThereWhenTheBuildHasThem)
{
    for (const auto& [c, cpp] :
         {std::pair(FFPSD_FORMAT_PNG, ffpsd::Format::kPng), std::pair(FFPSD_FORMAT_JPEG, ffpsd::Format::kJpeg),
          std::pair(FFPSD_FORMAT_WEBP, ffpsd::Format::kWebp)})
        EXPECT_EQ(ffpsd_format_is_supported(c) != 0, ffpsd::IsFormatSupported(cpp)) << c;

    const Owned<ffpsd_document_t> file = Open(kRgbPsd);
    const ffpsd_layer_t* layer = Layer(file, 1);
    const ffpsd::Bytes layer_pixels = Pixels(layer);
    Owned<ffpsd_image_t> pixels;
    ASSERT_EQ(ffpsd_layer_get_pixels(layer, pixels.out()), FFPSD_STATUS_OK);
    const ffpsd_image_view_t pixels_view = ViewOf(pixels);

    // Without a format, every call that takes one is UNSUPPORTED and leaves no handle.
    const auto expect_unsupported = [](ffpsd_status_t status, const void* out) {
        EXPECT_EQ(status, FFPSD_STATUS_UNSUPPORTED);
        EXPECT_EQ(out, nullptr);
    };

    Owned<ffpsd_buffer_t> png;
    const ffpsd_status_t png_status = ffpsd_png_save_memory(&pixels_view, FFPSD_PNG_COMPRESSION_FASTEST, png.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_PNG))
    {
        ASSERT_EQ(png_status, FFPSD_STATUS_OK) << ffpsd_last_error();
        Owned<ffpsd_image_t> image;
        ASSERT_EQ(
            ffpsd_png_load_memory(ffpsd_buffer_get_data(png), ffpsd_buffer_get_size(png), FFPSD_COLOR_MODE_RGB, 8, image.out()),
            FFPSD_STATUS_OK);
        EXPECT_EQ(Bytes(image), layer_pixels);
    }
    else
    {
        expect_unsupported(png_status, png);
    }

    Owned<ffpsd_buffer_t> jpeg;
    const ffpsd_status_t jpeg_status = ffpsd_jpeg_save_memory(&pixels_view, 90, jpeg.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_JPEG))
    {
        ASSERT_EQ(jpeg_status, FFPSD_STATUS_OK) << ffpsd_last_error();
        Owned<ffpsd_image_t> decoded;
        ASSERT_EQ(
            ffpsd_jpeg_load_memory(ffpsd_buffer_get_data(jpeg), ffpsd_buffer_get_size(jpeg), FFPSD_COLOR_MODE_RGB, 8, 1, decoded.out()),
            FFPSD_STATUS_OK)
            << ffpsd_last_error();
        const ffpsd_image_view_t view = ViewOf(decoded);
        EXPECT_EQ(view.channel_count, 3u);
        EXPECT_EQ(view.size, layer_pixels.size() / 4 * 3); // the layer's transparency is dropped
        EXPECT_EQ(ffpsd_jpeg_save_memory(&pixels_view, 0, jpeg.out()), FFPSD_STATUS_INVALID_ARGUMENT);
    }
    else
    {
        expect_unsupported(jpeg_status, jpeg);
    }

    Owned<ffpsd_buffer_t> webp;
    const ffpsd_status_t webp_status = ffpsd_webp_save_memory(&pixels_view, 50, 1, webp.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_WEBP))
    {
        ASSERT_EQ(webp_status, FFPSD_STATUS_OK) << ffpsd_last_error();
        Owned<ffpsd_image_t> lossless;
        ASSERT_EQ(
            ffpsd_webp_load_memory(ffpsd_buffer_get_data(webp), ffpsd_buffer_get_size(webp), FFPSD_COLOR_MODE_RGB, 8, lossless.out()),
            FFPSD_STATUS_OK)
            << ffpsd_last_error();

        // The layer is opaque, and WebP leaves out an alpha that is.
        const std::size_t colors = layer_pixels.size() / 4 * 3;
        ASSERT_TRUE(std::all_of(layer_pixels.begin() + static_cast<std::ptrdiff_t>(colors), layer_pixels.end(), [](std::uint8_t alpha) {
            return alpha == 0xFF;
        }));
        EXPECT_EQ(Bytes(lossless), ffpsd::Bytes(layer_pixels.data(), colors));
        EXPECT_EQ(ffpsd_webp_save_memory(&pixels_view, 101, 0, webp.out()), FFPSD_STATUS_INVALID_ARGUMENT);
    }
    else
    {
        expect_unsupported(webp_status, webp);
    }

    // A picture goes by its signature.
    Owned<ffpsd_image_t> picture;
    const std::vector<std::uint8_t> png_file = ReadFile(kRgbaPng);
    const ffpsd_status_t from_png = ffpsd_picture_load_memory(png_file.data(), png_file.size(), FFPSD_COLOR_MODE_RGB, 8, picture.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_PNG))
    {
        ASSERT_EQ(from_png, FFPSD_STATUS_OK) << ffpsd_last_error();
        const ffpsd_image_view_t view = ViewOf(picture);
        EXPECT_EQ(view.width, 3u);
        EXPECT_EQ(view.channel_count, 4u);
        EXPECT_EQ(view.color_mode, FFPSD_COLOR_MODE_RGB);
    }
    else
    {
        expect_unsupported(from_png, picture);
    }

    const ffpsd_status_t from_jpeg = ffpsd_picture_load(kQuadrantsJpeg.c_str(), FFPSD_COLOR_MODE_RGB, 8, picture.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_JPEG))
    {
        ASSERT_EQ(from_jpeg, FFPSD_STATUS_OK) << ffpsd_last_error();
        const ffpsd_image_view_t view = ViewOf(picture);
        EXPECT_EQ(view.width, 32u);
        EXPECT_EQ(view.channel_count, 3u);
    }
    else
    {
        expect_unsupported(from_jpeg, picture);
    }

    const ffpsd_status_t from_webp = ffpsd_picture_load(kQuadrantsWebp.c_str(), FFPSD_COLOR_MODE_GRAYSCALE, 16, picture.out());
    if (ffpsd_format_is_supported(FFPSD_FORMAT_WEBP))
    {
        ASSERT_EQ(from_webp, FFPSD_STATUS_OK) << ffpsd_last_error();
        const ffpsd_image_view_t view = ViewOf(picture);
        EXPECT_EQ(view.width, 32u);
        EXPECT_EQ(view.channel_count, 1u);
        EXPECT_EQ(view.depth, 16u);
    }
    else
    {
        expect_unsupported(from_webp, picture);
    }

    const std::uint8_t psd[] = {'8', 'B', 'P', 'S'};
    EXPECT_EQ(ffpsd_picture_load_memory(psd, sizeof(psd), FFPSD_COLOR_MODE_RGB, 8, picture.out()), FFPSD_STATUS_INVALID_FILE);
    EXPECT_EQ(ffpsd_picture_load(DataFile("no_such_file.png").c_str(), FFPSD_COLOR_MODE_RGB, 8, picture.out()), FFPSD_STATUS_IO);
    EXPECT_EQ(picture, nullptr);
}
