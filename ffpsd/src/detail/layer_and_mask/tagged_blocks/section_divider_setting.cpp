#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"

namespace ffpsd::detail
{
    template <> std::optional<SectionDividerSetting> ParseTaggedBlock<SectionDividerSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> type = ParseU32Block(data);
        if (!type.has_value())
            return std::nullopt;
        return SectionDividerSetting{*type};
    }
} // namespace ffpsd::detail
