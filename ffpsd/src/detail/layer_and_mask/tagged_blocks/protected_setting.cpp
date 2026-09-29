#include "detail/layer_and_mask/tagged_blocks/protected_setting.hpp"

namespace ffpsd::detail
{
    template <> std::optional<ProtectedSetting> DecodeTaggedBlock<ProtectedSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = DecodeU32(data);
        if (!value.has_value())
            return std::nullopt;
        return ProtectedSetting{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<ProtectedSetting>(const ProtectedSetting& value)
    {
        return EncodeU32(value.flags);
    }
} // namespace ffpsd::detail
