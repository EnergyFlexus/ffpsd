#include "detail/image_resources/document_specific_ids_seed_number.hpp"

#include "detail/io/big_endian_writer.hpp"

namespace ffpsd::detail
{
    std::uint32_t GetDocumentSpecificIdsSeedNumber(const ImageResources& image_resources)
    {
        const std::size_t at = FindImageResourceIndex(image_resources, ImageResourceId::kDocumentSpecificIdsSeedNumber);
        if (at == kNoImageResource || image_resources[at]->data.size() < sizeof(std::uint32_t))
            return 0;

        BigEndianReader reader(image_resources[at]->data);
        return reader.ReadU32();
    }

    void SetDocumentSpecificIdsSeedNumber(ImageResources& image_resources, std::uint32_t value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value);
        FindOrInsertImageResource(image_resources, ImageResourceId::kDocumentSpecificIdsSeedNumber).data = writer.Take();
    }
} // namespace ffpsd::detail
