#include "detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"

namespace ffpsd::detail
{
    template <> std::optional<UnicodeLayerName> DecodeTaggedBlock<UnicodeLayerName>(const std::vector<std::uint8_t>& data)
    {
        BigEndianReader reader(data);
        std::optional<std::string> name = TryReadUnicodeString(reader);
        if (!name.has_value())
            return std::nullopt;
        return UnicodeLayerName{std::move(*name)};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<UnicodeLayerName>(const UnicodeLayerName& value)
    {
        BigEndianWriter writer;
        WriteUnicodeString(writer, value.name);
        return writer.Take();
    }
} // namespace ffpsd::detail
