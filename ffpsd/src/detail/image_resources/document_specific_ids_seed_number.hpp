#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <cstdint>

namespace ffpsd::detail
{
    // Resource 1044: in practice the last layer id handed out; zero when missing.
    std::uint32_t GetDocumentSpecificIdsSeedNumber(const ImageResources& image_resources);
    void SetDocumentSpecificIdsSeedNumber(ImageResources& image_resources, std::uint32_t value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_
