#include "detail/image_resources/document_specific_ids_seed_number.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

namespace ffpsd::detail
{
    template <>
    std::optional<DocumentSpecificIdsSeedNumber> DecodeImageResource<DocumentSpecificIdsSeedNumber>(const std::vector<std::uint8_t>& data)
    {
        if (data.size() < sizeof(std::uint32_t))
            return std::nullopt;

        BigEndianReader reader(data);
        return DocumentSpecificIdsSeedNumber{reader.ReadU32()};
    }

    template <> std::vector<std::uint8_t> EncodeImageResource<DocumentSpecificIdsSeedNumber>(const DocumentSpecificIdsSeedNumber& value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value.value);
        return writer.Take();
    }
} // namespace ffpsd::detail
