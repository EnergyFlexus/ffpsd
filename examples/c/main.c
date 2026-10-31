/* Opens a PSD, adds a copy of the top layer, a picture with a mask and a Levels layer, grows the canvas and saves the result. */
#include <ffpsd/c_api.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#endif

/* Prints why the last call failed; true when it did. */
static int failed(ffpsd_status_t status)
{
    if (status == FFPSD_STATUS_OK)
        return 0;
    fprintf(stderr, "%s\n", ffpsd_last_error());
    return 1;
}

/* 0 when it worked; what it made is freed on every path. */
static int run(const char* input, const char* picture_path, const char* output)
{
    static const uint8_t hidden[100 * 100] = {0};
    ffpsd_image_view_t mask = {100, 100, 1, 8, FFPSD_COLOR_MODE_GRAYSCALE, hidden, sizeof(hidden)};
    ffpsd_levels_channel_t brighter = FFPSD_LEVELS_CHANNEL_IDENTITY;
    ffpsd_document_t* doc = NULL;
    ffpsd_image_t* merged = NULL;
    ffpsd_buffer_t* png = NULL;
    ffpsd_image_t* image = NULL;
    ffpsd_levels_t* levels = NULL;
    ffpsd_layer_t *top, *copy, *picture, *adjustment;
    ffpsd_image_view_t view;
    ffpsd_rect_t bounds;
    char name[256]; /* a longer name is cut to fit */
    uint32_t width;
    uint32_t height;
    int result = 1;

    if (failed(ffpsd_document_open(input, &doc)))
        return 1;
    width = ffpsd_document_get_width(doc);
    height = ffpsd_document_get_height(doc);
    printf(
        "%u x %u, %u bit, %u layers\n", (unsigned)width, (unsigned)height, (unsigned)ffpsd_document_get_depth(doc),
        (unsigned)ffpsd_document_get_layer_count(doc));

    /* The composite Photoshop saved with the file, as PNG. */
    if (failed(ffpsd_document_get_merged_image(doc, &merged)) || failed(ffpsd_image_get_view(merged, &view)) ||
        failed(ffpsd_png_save_memory(&view, FFPSD_PNG_COMPRESSION_BALANCED, &png)))
        goto done;
    printf("composite: %u bytes of PNG\n", (unsigned)ffpsd_buffer_get_size(png));

    /* Layers go from the bottom up, so the top one is the last. */
    if (failed(ffpsd_document_get_layer(doc, ffpsd_document_get_layer_count(doc) - 1, &top)) ||
        failed(ffpsd_layer_get_name(top, name, sizeof(name), NULL)) || failed(ffpsd_layer_get_bounds(top, &bounds)))
        goto done;
    printf("top layer: %s, %d x %d\n", name, bounds.right - bounds.left, bounds.bottom - bounds.top);

    /* A copy of it on top, half transparent, in Multiply mode. */
    if (failed(ffpsd_document_add_layer_copy(doc, top, &copy)) || failed(ffpsd_layer_set_name(copy, "Copy")) ||
        failed(ffpsd_layer_set_opacity(copy, 128)) || failed(ffpsd_layer_set_blend_key(copy, 0x6D756C20u))) /* 'mul ' */
        goto done;

    /* A PNG, JPEG or WebP at (50, 50), half the canvas, mirrored. */
    if (failed(ffpsd_picture_load(picture_path, ffpsd_document_get_color_mode(doc), ffpsd_document_get_depth(doc), &image)) ||
        failed(ffpsd_image_get_view(image, &view)) || failed(ffpsd_document_add_layer(doc, "Picture", &view, 50, 50, &picture)) ||
        failed(ffpsd_layer_resize(picture, width / 2, height / 2, FFPSD_RESAMPLE_FILTER_BICUBIC)) ||
        failed(ffpsd_layer_flip(picture, FFPSD_FLIP_HORIZONTAL)))
        goto done;

    /* A mask that hides a 100 x 100 square in its corner. */
    if (failed(ffpsd_layer_set_mask(picture, &mask, 50, 50, 255)))
        goto done;

    /* A Levels layer over everything that brightens the midtones. */
    brighter.gamma = 1.5;
    if (failed(ffpsd_levels_create(&levels)) || failed(ffpsd_levels_set_channel(levels, 0, &brighter)) ||
        failed(ffpsd_document_add_levels_layer(doc, "Levels", levels, &adjustment)))
        goto done;

    /* 100 px more canvas on every side, then save each channel RLE or raw, whichever is smaller. */
    if (failed(ffpsd_document_resize_canvas(doc, width + 200, height + 200, FFPSD_ANCHOR_CENTER)) ||
        failed(ffpsd_document_save(doc, output, FFPSD_COMPRESSION_RLE_OR_RAW)))
        goto done;
    printf("saved %u layers to %s\n", (unsigned)ffpsd_document_get_layer_count(doc), output);
    result = 0;

done:
    /* The document owns its layers, so this frees them too. */
    ffpsd_levels_destroy(levels);
    ffpsd_image_destroy(image);
    ffpsd_buffer_destroy(png);
    ffpsd_image_destroy(merged);
    ffpsd_document_destroy(doc);
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "usage: ffpsd_example_c <in.psd> <picture> <out.psd>\n");
        return 2;
    }

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); /* layer names are UTF-8 */
#endif

    return run(argv[1], argv[2], argv[3]);
}
