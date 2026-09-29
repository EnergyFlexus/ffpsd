#ifndef FFPSD_C_API_H_
#define FFPSD_C_API_H_

#include <ffpsd/export.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Every call that can fail returns a status; the rest return 0 for a NULL handle. */
    typedef enum ffpsd_status_t
    {
        FFPSD_STATUS_OK = 0,
        FFPSD_STATUS_INVALID_ARGUMENT = 1,
        FFPSD_STATUS_OUT_OF_RANGE = 2,
        FFPSD_STATUS_NOT_FOUND = 3,
        FFPSD_STATUS_INVALID_FILE = 4,
        FFPSD_STATUS_IO = 5,
        FFPSD_STATUS_INVALID_OPERATION = 6,
        FFPSD_STATUS_UNSUPPORTED = 7,
        FFPSD_STATUS_OUT_OF_MEMORY = 8,
        FFPSD_STATUS_UNKNOWN = 9
    } ffpsd_status_t;

    typedef enum ffpsd_color_mode_t
    {
        FFPSD_COLOR_MODE_BITMAP = 0,
        FFPSD_COLOR_MODE_GRAYSCALE = 1,
        FFPSD_COLOR_MODE_INDEXED = 2,
        FFPSD_COLOR_MODE_RGB = 3,
        FFPSD_COLOR_MODE_CMYK = 4,
        FFPSD_COLOR_MODE_MULTICHANNEL = 7,
        FFPSD_COLOR_MODE_DUOTONE = 8,
        FFPSD_COLOR_MODE_LAB = 9
    } ffpsd_color_mode_t;

    /* A group takes two records; stored bottom to top, its end marker comes first. */
    typedef enum ffpsd_layer_kind_t
    {
        FFPSD_LAYER_KIND_RASTER = 0,
        FFPSD_LAYER_KIND_GROUP_OPEN = 1,
        FFPSD_LAYER_KIND_GROUP_CLOSED = 2,
        FFPSD_LAYER_KIND_GROUP_END = 3,
        FFPSD_LAYER_KIND_ADJUSTMENT = 4
    } ffpsd_layer_kind_t;

    /* How a save writes pixel data; RLE is what Photoshop writes. */
    typedef enum ffpsd_compression_t
    {
        FFPSD_COMPRESSION_RAW = 0,
        FFPSD_COMPRESSION_RLE = 1
    } ffpsd_compression_t;

    typedef enum ffpsd_resample_filter_t
    {
        FFPSD_RESAMPLE_FILTER_NEAREST = 0,
        FFPSD_RESAMPLE_FILTER_BICUBIC = 1
    } ffpsd_resample_filter_t;

    /* Owned by the caller, released with its _destroy. */
    typedef struct ffpsd_document_t ffpsd_document_t;
    typedef struct ffpsd_image_t ffpsd_image_t;
    typedef struct ffpsd_buffer_t ffpsd_buffer_t;
    typedef struct ffpsd_version_info_t ffpsd_version_info_t;
    typedef struct ffpsd_levels_t ffpsd_levels_t;

    /* Owned by its document; valid until the layer is removed or the document destroyed. */
    typedef struct ffpsd_layer_t ffpsd_layer_t;

    typedef struct ffpsd_rect_t
    {
        int32_t top;
        int32_t left;
        int32_t bottom;
        int32_t right;
    } ffpsd_rect_t;

    /* Planar samples in native byte order; as an input it only has to live for the call. */
    typedef struct ffpsd_image_view_t
    {
        uint32_t width;
        uint32_t height;
        uint16_t channel_count;
        uint16_t depth;
        const uint8_t* data;
        size_t size;
    } ffpsd_image_view_t;

    /* Resource 1005; resolutions in pixels per inch. */
    typedef struct ffpsd_resolution_info_t
    {
        double horizontal;
        int16_t horizontal_unit;
        int16_t width_unit;
        double vertical;
        int16_t vertical_unit;
        int16_t height_unit;
    } ffpsd_resolution_info_t;

    /* Borrowed: from a get until the resource changes, into a set for the call. */
    typedef struct ffpsd_image_resource_t
    {
        uint16_t id;
        const char* name;
        const uint8_t* data;
        size_t size;
    } ffpsd_image_resource_t;

    /* Borrowed like ffpsd_image_resource_t. */
    typedef struct ffpsd_tagged_block_t
    {
        uint32_t signature;
        uint32_t key;
        const uint8_t* data;
        size_t size;
    } ffpsd_tagged_block_t;

    typedef struct ffpsd_levels_channel_t
    {
        uint16_t input_floor;
        uint16_t input_ceiling;
        uint16_t output_floor;
        uint16_t output_ceiling;
        double gamma;
    } ffpsd_levels_channel_t;

    /* A record that changes nothing. */
#define FFPSD_LEVELS_CHANNEL_IDENTITY {0, 255, 0, 255, 1.0}

    /* Library version as "MAJOR.MINOR.PATCH". */
    FFPSD_EXPORT const char* ffpsd_version(void);

    /* Why the last call on this thread failed; empty after a call that succeeded. */
    FFPSD_EXPORT const char* ffpsd_last_error(void);

    FFPSD_EXPORT const uint8_t* ffpsd_buffer_get_data(const ffpsd_buffer_t* buffer);
    FFPSD_EXPORT size_t ffpsd_buffer_get_size(const ffpsd_buffer_t* buffer);
    FFPSD_EXPORT void ffpsd_buffer_destroy(ffpsd_buffer_t* buffer);

    /* The view points into the image and lives as long as it. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_image_get_view(const ffpsd_image_t* image, ffpsd_image_view_t* out);
    FFPSD_EXPORT void ffpsd_image_destroy(ffpsd_image_t* image);

    /* Resource 1057. Names are UTF-8 and live until the next set of that name or the destroy. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_create(ffpsd_version_info_t** out);
    FFPSD_EXPORT void ffpsd_version_info_destroy(ffpsd_version_info_t* info);
    FFPSD_EXPORT uint32_t ffpsd_version_info_get_version(const ffpsd_version_info_t* info);
    FFPSD_EXPORT int ffpsd_version_info_get_has_real_merged_data(const ffpsd_version_info_t* info);
    FFPSD_EXPORT const char* ffpsd_version_info_get_writer_name(const ffpsd_version_info_t* info);
    FFPSD_EXPORT const char* ffpsd_version_info_get_reader_name(const ffpsd_version_info_t* info);
    FFPSD_EXPORT uint32_t ffpsd_version_info_get_file_version(const ffpsd_version_info_t* info);
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_set_version(ffpsd_version_info_t* info, uint32_t version);
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_set_has_real_merged_data(ffpsd_version_info_t* info, int value);
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_set_writer_name(ffpsd_version_info_t* info, const char* name);
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_set_reader_name(ffpsd_version_info_t* info, const char* name);
    FFPSD_EXPORT ffpsd_status_t ffpsd_version_info_set_file_version(ffpsd_version_info_t* info, uint32_t version);

    /* Record 0 is every color channel at once, record 1 channel 0 and so on. A new one has none. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_levels_create(ffpsd_levels_t** out);
    FFPSD_EXPORT void ffpsd_levels_destroy(ffpsd_levels_t* levels);
    FFPSD_EXPORT size_t ffpsd_levels_get_count(const ffpsd_levels_t* levels);
    FFPSD_EXPORT ffpsd_status_t ffpsd_levels_get_channel(const ffpsd_levels_t* levels, size_t index, ffpsd_levels_channel_t* out);

    /* Past the end, the records in between are added as the identity. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_levels_set_channel(ffpsd_levels_t* levels, size_t index, const ffpsd_levels_channel_t* channel);

    /* Paths are passed to the C++ API as they are, in the system's narrow encoding. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_create(ffpsd_document_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_open(const char* path, ffpsd_document_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_open_memory(const uint8_t* data, size_t size, ffpsd_document_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_save(const ffpsd_document_t* doc, const char* path, ffpsd_compression_t compression);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_save_memory(const ffpsd_document_t* doc, ffpsd_compression_t compression, ffpsd_buffer_t** out);
    FFPSD_EXPORT void ffpsd_document_destroy(ffpsd_document_t* doc);

    FFPSD_EXPORT uint32_t ffpsd_document_get_width(const ffpsd_document_t* doc);
    FFPSD_EXPORT uint32_t ffpsd_document_get_height(const ffpsd_document_t* doc);
    FFPSD_EXPORT uint16_t ffpsd_document_get_channel_count(const ffpsd_document_t* doc);
    FFPSD_EXPORT uint16_t ffpsd_document_get_depth(const ffpsd_document_t* doc);
    FFPSD_EXPORT ffpsd_color_mode_t ffpsd_document_get_color(const ffpsd_document_t* doc);
    FFPSD_EXPORT int ffpsd_document_is_psb(const ffpsd_document_t* doc);
    FFPSD_EXPORT int ffpsd_document_get_has_real_merged_data(const ffpsd_document_t* doc);

    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_width(ffpsd_document_t* doc, uint32_t width);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_height(ffpsd_document_t* doc, uint32_t height);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_channel_count(ffpsd_document_t* doc, uint16_t channel_count);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_depth(ffpsd_document_t* doc, uint16_t depth);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_color(ffpsd_document_t* doc, ffpsd_color_mode_t color);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_psb(ffpsd_document_t* doc, int psb);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_has_real_merged_data(ffpsd_document_t* doc, int value);

    /* 72 dpi when the file has no resource 1005. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_get_resolution_info(const ffpsd_document_t* doc, ffpsd_resolution_info_t* out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_resolution_info(ffpsd_document_t* doc, const ffpsd_resolution_info_t* info);

    FFPSD_EXPORT ffpsd_status_t ffpsd_document_get_version_info(const ffpsd_document_t* doc, ffpsd_version_info_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_version_info(ffpsd_document_t* doc, const ffpsd_version_info_t* info);

    /* A NULL name is an empty one. A new id is inserted in id order. */
    FFPSD_EXPORT size_t ffpsd_document_get_image_resource_count(const ffpsd_document_t* doc);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_get_image_resource_by_index(const ffpsd_document_t* doc, size_t index, ffpsd_image_resource_t* out);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_get_image_resource_by_id(const ffpsd_document_t* doc, uint16_t id, ffpsd_image_resource_t* out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_image_resource(ffpsd_document_t* doc, const ffpsd_image_resource_t* resource);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_remove_image_resource(ffpsd_document_t* doc, uint16_t id);

    /* Bottom to top; a stack change drops resources 1024, 1026, 1072 and the real composite flag. */
    FFPSD_EXPORT size_t ffpsd_document_get_layer_count(const ffpsd_document_t* doc);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_get_layer(ffpsd_document_t* doc, size_t index, ffpsd_layer_t** out);

    /* A raster layer on top; a NULL image is an empty layer. The name is UTF-8. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_add_layer(
        ffpsd_document_t* doc, const char* name, const ffpsd_image_view_t* image, int32_t top, int32_t left, ffpsd_layer_t** out);

    /* Photoshop's locked background: at the bottom, the document's size, one at most. */
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_add_background_layer(ffpsd_document_t* doc, const char* name, const ffpsd_image_view_t* image, ffpsd_layer_t** out);

    /* A raster layer without a mask becomes the background, over white and fitted to the canvas. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_background_layer(ffpsd_document_t* doc, size_t index);

    /* The background becomes an ordinary layer where it is; NOT_FOUND when there is none. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_unset_background_layer(ffpsd_document_t* doc);

    /* NULL levels change nothing. */
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_add_levels_layer(ffpsd_document_t* doc, const char* name, const ffpsd_levels_t* levels, ffpsd_layer_t** out);

    /* From this document or one of the same depth, color mode and format. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_add_layer_copy(ffpsd_document_t* doc, const ffpsd_layer_t* source, ffpsd_layer_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_remove_layer(ffpsd_document_t* doc, size_t index);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_move_layer(ffpsd_document_t* doc, size_t from, size_t to);

    /* Section 4 blocks after the layers; the same rules as a layer's. */
    FFPSD_EXPORT size_t ffpsd_document_get_tagged_block_count(const ffpsd_document_t* doc);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_get_tagged_block_by_index(const ffpsd_document_t* doc, size_t index, ffpsd_tagged_block_t* out);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_document_get_tagged_block_by_key(const ffpsd_document_t* doc, uint32_t key, ffpsd_tagged_block_t* out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_tagged_block(ffpsd_document_t* doc, const ffpsd_tagged_block_t* block);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_remove_tagged_block(ffpsd_document_t* doc, uint32_t key);

    /* An empty image when the file has none. Setting it sets has_real_merged_data. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_get_merged_image(const ffpsd_document_t* doc, ffpsd_image_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_document_set_merged_image(ffpsd_document_t* doc, const ffpsd_image_view_t* image);

    FFPSD_EXPORT ffpsd_layer_kind_t ffpsd_layer_get_kind(const ffpsd_layer_t* layer);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_bounds(const ffpsd_layer_t* layer, ffpsd_rect_t* out);

    /* UTF-8, cut to fit and null-terminated; length gets the full size. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_name(const ffpsd_layer_t* layer, char* buffer, size_t capacity, size_t* length);

    FFPSD_EXPORT uint8_t ffpsd_layer_get_opacity(const ffpsd_layer_t* layer);
    FFPSD_EXPORT int ffpsd_layer_is_visible(const ffpsd_layer_t* layer);
    FFPSD_EXPORT int ffpsd_layer_is_background(const ffpsd_layer_t* layer);
    FFPSD_EXPORT uint32_t ffpsd_layer_get_blend_key(const ffpsd_layer_t* layer);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_opacity(ffpsd_layer_t* layer, uint8_t opacity);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_visible(ffpsd_layer_t* layer, int visible);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_blend_key(ffpsd_layer_t* layer, uint32_t blend_key);

    /* The block of an adjustment layer, such as 'levl', or 0. */
    FFPSD_EXPORT uint32_t ffpsd_layer_get_adjustment_key(const ffpsd_layer_t* layer);

    /* NOT_FOUND unless this is a Levels layer. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_levels(const ffpsd_layer_t* layer, ffpsd_levels_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_levels(ffpsd_layer_t* layer, const ffpsd_levels_t* levels);

    /* A raster layer without a mask; the background does not move or resize. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_position(ffpsd_layer_t* layer, int32_t top, int32_t left);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_resize(ffpsd_layer_t* layer, uint32_t width, uint32_t height, ffpsd_resample_filter_t filter);

    /* Color planes by channel id, then transparency when the layer has one. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_pixels(const ffpsd_layer_t* layer, ffpsd_image_t** out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_pixels(ffpsd_layer_t* layer, const ffpsd_image_view_t* image);

    /* Unchecked; keys may repeat, so a get or remove by key finds the first. */
    FFPSD_EXPORT size_t ffpsd_layer_get_tagged_block_count(const ffpsd_layer_t* layer);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_tagged_block_by_index(const ffpsd_layer_t* layer, size_t index, ffpsd_tagged_block_t* out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_get_tagged_block_by_key(const ffpsd_layer_t* layer, uint32_t key, ffpsd_tagged_block_t* out);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_set_tagged_block(ffpsd_layer_t* layer, const ffpsd_tagged_block_t* block);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_remove_tagged_block(ffpsd_layer_t* layer, uint32_t key);

    /* The layer's pixels at its own size; gray and RGB documents only. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_save_as_png(const ffpsd_layer_t* layer, const char* path);
    FFPSD_EXPORT ffpsd_status_t ffpsd_layer_save_as_png_memory(const ffpsd_layer_t* layer, ffpsd_buffer_t** out);

    /* Every PNG call is UNSUPPORTED in a build without PNG. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_png_load(const char* path, ffpsd_color_mode_t color, uint16_t depth, ffpsd_image_t** out);
    FFPSD_EXPORT ffpsd_status_t
    ffpsd_png_load_memory(const uint8_t* data, size_t size, ffpsd_color_mode_t color, uint16_t depth, ffpsd_image_t** out);

    /* Gray, gray with alpha, RGB or RGBA by the channel count; 8 or 16 bit. */
    FFPSD_EXPORT ffpsd_status_t ffpsd_png_save(const ffpsd_image_view_t* image, const char* path);
    FFPSD_EXPORT ffpsd_status_t ffpsd_png_save_memory(const ffpsd_image_view_t* image, ffpsd_buffer_t** out);

#ifdef __cplusplus
}
#endif

#endif /* FFPSD_C_API_H_ */
