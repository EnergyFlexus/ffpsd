/* Built by the C compiler: anything C++ in c_api.h breaks this file, not a test. */
#include <ffpsd/c_api.h>

int ffpsd_test_count_layers_from_c(const char* path);

int ffpsd_test_count_layers_from_c(const char* path)
{
    ffpsd_document_t* doc = NULL;
    ffpsd_levels_channel_t identity = FFPSD_LEVELS_CHANNEL_IDENTITY;
    ffpsd_image_view_t view = {0, 0, 0, 8, FFPSD_COLOR_MODE_RGB, NULL, 0};
    int count = -1;

    if (ffpsd_document_open(path, &doc) != FFPSD_STATUS_OK)
        return -1;
    if (identity.input_ceiling == 255 && view.depth == 8)
        count = (int)ffpsd_document_get_layer_count(doc);

    ffpsd_document_destroy(doc);
    return count;
}
