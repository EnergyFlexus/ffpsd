#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"

#include "detail/io/big_endian_reader.hpp"

namespace ffpsd::detail
{
    template <> std::optional<SectionDividerSetting> DecodeTaggedBlock<SectionDividerSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> type = BigEndianReader(data).TryReadU32();
        if (!type.has_value())
            return std::nullopt;
        return SectionDividerSetting{*type};
    }
} // namespace ffpsd::detail
