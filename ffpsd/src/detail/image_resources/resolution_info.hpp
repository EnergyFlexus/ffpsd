#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <ffpsd/image_resources.hpp>

namespace ffpsd::detail
{
    ResolutionInfo GetResolutionInfo(const ImageResources& image_resources);
    void SetResolutionInfo(ImageResources& image_resources, ResolutionInfo value);

} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_
