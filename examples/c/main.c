/* Opens a PSD, prints what it holds, copies the top layer and saves the result. */
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

int main(int argc, char** argv)
{
    ffpsd_document_t* doc = NULL;
    ffpsd_layer_t* top = NULL;
    ffpsd_layer_t* copy = NULL;
    size_t count;
    size_t i;
    int result = 1;

    if (argc != 3)
    {
        fprintf(stderr, "usage: ffpsd_example_c <in.psd> <out.psd>\n");
        return 2;
    }

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8); /* layer names are UTF-8 */
#endif

    /* Open the document. */
    if (failed(ffpsd_document_open(argv[1], &doc)))
        return 1;

    /* Read what it holds; layers go from the bottom up. */
    count = ffpsd_document_get_layer_count(doc);
    printf(
        "%u x %u, %u bit, %u layers\n", (unsigned)ffpsd_document_get_width(doc), (unsigned)ffpsd_document_get_height(doc),
        (unsigned)ffpsd_document_get_depth(doc), (unsigned)count);
    for (i = 0; i < count; ++i)
    {
        ffpsd_layer_t* layer = NULL;
        ffpsd_rect_t bounds;
        char name[256]; /* a longer name is cut to fit */

        if (failed(ffpsd_document_get_layer(doc, i, &layer)) || failed(ffpsd_layer_get_name(layer, name, sizeof(name), NULL)) ||
            failed(ffpsd_layer_get_bounds(layer, &bounds)))
            goto done;
        printf("  %u: %s, %d x %d\n", (unsigned)i, name, bounds.right - bounds.left, bounds.bottom - bounds.top);
    }

    /* Put a copy of the top layer on top; a group marker alone cannot be copied. */
    if (failed(ffpsd_document_get_layer(doc, count - 1, &top)) || failed(ffpsd_document_add_layer_copy(doc, top, &copy)))
        goto done;

    /* Save, packed with RLE as Photoshop does. */
    if (failed(ffpsd_document_save(doc, argv[2], FFPSD_COMPRESSION_RLE)))
        goto done;
    printf("saved %u layers to %s\n", (unsigned)ffpsd_document_get_layer_count(doc), argv[2]);
    result = 0;

done:
    /* The document owns its layers, so this frees them too. */
    ffpsd_document_destroy(doc);
    return result;
}
