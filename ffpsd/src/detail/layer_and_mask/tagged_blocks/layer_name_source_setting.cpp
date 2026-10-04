#include "detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

namespace ffpsd::detail
{
    template <> std::optional<LayerNameSourceSetting> DecodeTaggedBlock<LayerNameSourceSetting>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = BigEndianReader(data).TryReadU32();
        if (!value.has_value())
            return std::nullopt;
        return LayerNameSourceSetting{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerNameSourceSetting>(const LayerNameSourceSetting& value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value.id);
        return writer.Take();
    }
} // namespace ffpsd::detail
