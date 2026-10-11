#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <cstdint>
#include <ffpsd/image_resources.hpp>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // Empty when the block is shorter than its 16 bytes.
    template <> std::optional<ResolutionInfo> DecodeImageResource<ResolutionInfo>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeImageResource<ResolutionInfo>(const ResolutionInfo& value);

    // Above 0 and at most 32767, which signed 16.16 holds.
    void CheckResolution(double dpi);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_RESOLUTION_INFO_HPP_
