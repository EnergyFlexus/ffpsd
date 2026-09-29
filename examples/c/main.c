/* Opens a PSD, prints what it holds, puts a copy of its top layer on top and saves the result. */
#include <ffpsd/c_api.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#endif

/* Prints why the last call failed; true when it did. */
static int failed(ffpsd_status_t status, const char* what)
{
    if (status == FFPSD_STATUS_OK)
        return 0;
    fprintf(stderr, "%s: %s\n", what, ffpsd_last_error());
    return 1;
}

static const char* color_name(ffpsd_color_mode_t color)
{
    switch (color)
    {
    case FFPSD_COLOR_MODE_GRAYSCALE:
        return "grayscale";
    case FFPSD_COLOR_MODE_RGB:
        return "RGB";
    case FFPSD_COLOR_MODE_CMYK:
        return "CMYK";
    case FFPSD_COLOR_MODE_LAB:
        return "Lab";
    default:
        return "other";
    }
}

static const char* kind_name(ffpsd_layer_kind_t kind)
{
    switch (kind)
    {
    case FFPSD_LAYER_KIND_RASTER:
        return "raster";
    case FFPSD_LAYER_KIND_ADJUSTMENT:
        return "adjustment";
    case FFPSD_LAYER_KIND_GROUP_END:
        return "group end";
    default:
        return "group";
    }
}

static int print_document(ffpsd_document_t* doc)
{
    ffpsd_resolution_info_t resolution;
    ffpsd_version_info_t* version = NULL;
    size_t i;

    printf(
        "%u x %u, %s, %u bit%s\n", (unsigned)ffpsd_document_get_width(doc), (unsigned)ffpsd_document_get_height(doc),
        color_name(ffpsd_document_get_color(doc)), (unsigned)ffpsd_document_get_depth(doc), ffpsd_document_is_psb(doc) ? ", PSB" : "");

    if (failed(ffpsd_document_get_resolution_info(doc, &resolution), "resolution"))
        return 0;
    printf("resolution: %g x %g ppi\n", resolution.horizontal, resolution.vertical);

    if (failed(ffpsd_document_get_version_info(doc, &version), "version info"))
        return 0;
    printf("written by: %s, for %s\n", ffpsd_version_info_get_writer_name(version), ffpsd_version_info_get_reader_name(version));
    ffpsd_version_info_destroy(version);

    printf("layers, bottom to top: %u\n", (unsigned)ffpsd_document_get_layer_count(doc));
    for (i = 0; i < ffpsd_document_get_layer_count(doc); ++i)
    {
        ffpsd_layer_t* layer = NULL;
        ffpsd_rect_t bounds;
        char name[256];

        /* A longer name is cut to fit, still null-terminated. */
        if (failed(ffpsd_document_get_layer(doc, i, &layer), "layer") ||
            failed(ffpsd_layer_get_name(layer, name, sizeof(name), NULL), "layer name") ||
            failed(ffpsd_layer_get_bounds(layer, &bounds), "layer bounds"))
            return 0;

        printf(
            "  %u: %s (%s), %d x %d at %d, %d%s\n", (unsigned)i, name, kind_name(ffpsd_layer_get_kind(layer)), bounds.right - bounds.left,
            bounds.bottom - bounds.top, bounds.left, bounds.top, ffpsd_layer_is_visible(layer) ? "" : ", hidden");
    }
    return 1;
}

/* A copy of one group marker would break the nesting, so the top layer that is not one. */
static ffpsd_layer_t* top_layer(ffpsd_document_t* doc)
{
    size_t i = ffpsd_document_get_layer_count(doc);
    while (i-- > 0)
    {
        ffpsd_layer_t* layer = NULL;
        if (failed(ffpsd_document_get_layer(doc, i, &layer), "layer"))
            return NULL;

        const ffpsd_layer_kind_t kind = ffpsd_layer_get_kind(layer);
        if (kind == FFPSD_LAYER_KIND_RASTER || kind == FFPSD_LAYER_KIND_ADJUSTMENT)
            return layer;
    }
    return NULL;
}

int main(int argc, char** argv)
{
    ffpsd_document_t* doc = NULL;
    ffpsd_layer_t* top = NULL;
    ffpsd_layer_t* copy = NULL;
    int ok = 0;

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); /* layer names are UTF-8 */
#endif
    if (argc != 3)
    {
        fprintf(stderr, "usage: ffpsd_example_c <in.psd> <out.psd>\n");
        return 2;
    }

    if (failed(ffpsd_document_open(argv[1], &doc), argv[1]))
        return 1;

    if (print_document(doc))
    {
        top = top_layer(doc);
        if (top == NULL)
            fprintf(stderr, "no layer to copy\n");
        else if (
            !failed(ffpsd_document_add_layer_copy(doc, top, &copy), "copy") &&
            !failed(ffpsd_document_save(doc, argv[2], FFPSD_COMPRESSION_RLE), argv[2]))
        {
            printf("\ncopied the top layer, saved %u layers to %s\n", (unsigned)ffpsd_document_get_layer_count(doc), argv[2]);
            ok = 1;
        }
    }

    ffpsd_document_destroy(doc);
    return ok ? 0 : 1;
}
