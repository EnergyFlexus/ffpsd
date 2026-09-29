#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

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

    // The first resource with this id, or null; a linear scan, as a file holds a few dozen.
    const ImageResource* FindImageResource(const ImageResources& image_resources, std::uint16_t id) noexcept;
    ImageResource* FindImageResource(ImageResources& image_resources, std::uint16_t id) noexcept;

    // A missing id is inserted in ascending order, as Photoshop keeps them.
    ImageResource& FindOrInsertImageResource(ImageResources& image_resources, std::uint16_t id);

    // Specialized by each resource ffpsd interprets, for its struct with kId; empty when the data cannot be read.
    template <class T> std::optional<T> DecodeImageResource(const std::vector<std::uint8_t>& data);
    template <class T> std::vector<std::uint8_t> EncodeImageResource(const T& value);

    // The resource with T's id, read; empty when there is none or it cannot be read.
    template <class T> std::optional<T> GetImageResource(const ImageResources& image_resources)
    {
        const ImageResource* resource = FindImageResource(image_resources, T::kId);
        if (resource == nullptr)
            return std::nullopt;
        return DecodeImageResource<T>(resource->data);
    }

    // Assigns in place to the resource with T's id, otherwise inserts one.
    template <class T> void SetImageResource(ImageResources& image_resources, const T& value)
    {
        std::vector<std::uint8_t> data = EncodeImageResource(value);
        FindOrInsertImageResource(image_resources, T::kId).data = std::move(data);
    }
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_IMAGE_RESOURCE_HPP_
