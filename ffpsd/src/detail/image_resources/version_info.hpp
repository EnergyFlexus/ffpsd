#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <cstdint>
#include <ffpsd/image_resources.hpp>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // Empty when the block is too short for a version and the flag; what cannot be read keeps its default.
    template <> std::optional<VersionInfo> DecodeImageResource<VersionInfo>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeImageResource<VersionInfo>(const VersionInfo& value);

    // The flag byte alone: reading it allocates nothing, and writing it keeps the names of the file's writer.
    bool HasRealMergedData(const ImageResources& image_resources);
    void SetHasRealMergedData(ImageResources& image_resources, bool value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_VERSION_INFO_HPP_
