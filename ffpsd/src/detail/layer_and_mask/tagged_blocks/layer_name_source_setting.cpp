#include "detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp"

namespace ffpsd::detail
{
    template <> std::optional<LayerNameSourceSetting> DecodeTaggedBlock<LayerNameSourceSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = DecodeU32(data);
        if (!value.has_value())
            return std::nullopt;
        return LayerNameSourceSetting{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerNameSourceSetting>(const LayerNameSourceSetting& value)
    {
        return EncodeU32(value.id);
    }
} // namespace ffpsd::detail
