#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <ffpsd/image_resources.hpp>

namespace ffpsd::detail
{
    VersionInfo GetVersionInfo(const ImageResources& image_resources);
    void SetVersionInfo(ImageResources& image_resources, const VersionInfo& info);

    bool GetHasRealMergedData(const ImageResources& image_resources);
    void SetHasRealMergedData(ImageResources& image_resources, bool value);

} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_
