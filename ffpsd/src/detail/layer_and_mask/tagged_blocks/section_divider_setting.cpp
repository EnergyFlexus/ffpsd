#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"

namespace ffpsd::detail
{
    template <> std::optional<SectionDividerSetting> DecodeTaggedBlock<SectionDividerSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> type = DecodeU32(data);
        if (!type.has_value())
            return std::nullopt;
        return SectionDividerSetting{*type};
    }
} // namespace ffpsd::detail
