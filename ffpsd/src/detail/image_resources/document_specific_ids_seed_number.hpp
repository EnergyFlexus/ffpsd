#ifndef FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_
#define FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_

#include "detail/image_resources/image_resource.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // In practice the last layer id handed out.
    struct DocumentSpecificIdsSeedNumber
    {
        static constexpr std::uint16_t kId = 1044;
        std::uint32_t value = 0;
    };

    template <>
    std::optional<DocumentSpecificIdsSeedNumber> ParseImageResource<DocumentSpecificIdsSeedNumber>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeImageResource<DocumentSpecificIdsSeedNumber>(const DocumentSpecificIdsSeedNumber& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_RESOURCES_DOCUMENT_SPECIFIC_IDS_SEED_NUMBER_HPP_
