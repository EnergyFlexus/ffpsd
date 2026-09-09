#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image_resources.hpp>
#include <memory>
#include <vector>

namespace ffpsd::detail
{
    // Unscoped, so an enumerator converts to std::uint16_t wherever an id is taken.
    enum ImageResourceId : std::uint16_t
    {
        kResolutionInfo = 1005,
        kLayerStateInformation = 1024,
        kLayersGroupInformation = 1026,
        kDocumentSpecificIdsSeedNumber = 1044,
        kVersionInfo = 1057,
        kLayerGroupsEnabledId = 1072
    };

    // One allocation per block, so pointers survive inserts and removals of others.
    using ImageResources = std::vector<std::unique_ptr<ImageResource>>;

    ImageResources ParseImageResources(BigEndianReader& reader);
    void WriteImageResources(BigEndianWriter& writer, const ImageResources& image_resources);

    constexpr std::size_t kNoImageResource = static_cast<std::size_t>(-1);

    // A linear scan: a file holds a few dozen blocks.
    std::size_t FindImageResourceIndex(const ImageResources& image_resources, std::uint16_t id);

    // A missing id is inserted in ascending order, as Photoshop keeps them.
    ImageResource& FindOrInsertImageResource(ImageResources& image_resources, std::uint16_t id);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
