#include "detail/layer_and_mask/tagged_blocks/protected_setting.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

namespace ffpsd::detail
{
    template <> std::optional<ProtectedSetting> DecodeTaggedBlock<ProtectedSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = BigEndianReader(data).TryReadU32();
        if (!value.has_value())
            return std::nullopt;
        return ProtectedSetting{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<ProtectedSetting>(const ProtectedSetting& value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value.flags);
        return writer.Take();
    }
} // namespace ffpsd::detail
