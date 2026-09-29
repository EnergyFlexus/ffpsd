#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image_resources.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    // Resources ffpsd drops but does not read; the ones it reads carry kId in their struct.
    enum ImageResourceId : std::uint16_t
    {
        kLayerStateInformation = 1024,
        kLayersGroupInformation = 1026,
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

    // Specialized by each resource ffpsd interprets, for its struct with kId; empty when the data cannot be read.
    template <class T> std::optional<T> ParseImageResource(const std::vector<std::uint8_t>& data);
    template <class T> std::vector<std::uint8_t> EncodeImageResource(const T& value);

    // The resource with T's id, read; empty when there is none or it cannot be read.
    template <class T> std::optional<T> GetImageResource(const ImageResources& image_resources)
    {
        const std::size_t at = FindImageResourceIndex(image_resources, T::kId);
        if (at == kNoImageResource)
            return std::nullopt;
        return ParseImageResource<T>(image_resources[at]->data);
    }

    // Assigns in place to the resource with T's id, otherwise inserts one.
    template <class T> void SetImageResource(ImageResources& image_resources, const T& value)
    {
        std::vector<std::uint8_t> data = EncodeImageResource(value);
        FindOrInsertImageResource(image_resources, T::kId).data = std::move(data);
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
