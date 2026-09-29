#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"

namespace ffpsd::detail
{
    template <> std::optional<UnicodeLayerName> DecodeTaggedBlock<UnicodeLayerName>(const std::vector<std::uint8_t>& data)
    {
        BigEndianReader reader(data);
        if (reader.GetRemaining() < sizeof(std::uint32_t) ||
            reader.PeekU32() > (reader.GetRemaining() - sizeof(std::uint32_t)) / sizeof(std::uint16_t))
            return std::nullopt;
        return UnicodeLayerName{ReadUnicodeString(reader)};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<UnicodeLayerName>(const UnicodeLayerName& value)
    {
        BigEndianWriter writer;
        WriteUnicodeString(writer, value.name);
        return writer.Take();
    }
} // namespace ffpsd::detail
