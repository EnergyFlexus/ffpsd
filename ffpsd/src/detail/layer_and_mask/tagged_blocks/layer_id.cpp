#include "detail/layer_and_mask/tagged_blocks/layer_id.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

namespace ffpsd::detail
{
    template <> std::optional<LayerId> DecodeTaggedBlock<LayerId>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = BigEndianReader(data).TryReadU32();
        if (!value.has_value())
            return std::nullopt;
        return LayerId{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerId>(const LayerId& value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value.id);
        return writer.Take();
    }
} // namespace ffpsd::detail
