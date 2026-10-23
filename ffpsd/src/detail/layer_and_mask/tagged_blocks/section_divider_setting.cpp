#include "detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/signatures.hpp"

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kOrdinaryGroup = 0;
    } // namespace

    template <> std::optional<SectionDividerSetting> DecodeTaggedBlock<SectionDividerSetting>(const std::vector<std::uint8_t>& data)
    {
        BigEndianReader reader(data);
        const std::optional<std::uint32_t> type = reader.TryReadU32();
        if (!type.has_value())
            return std::nullopt;

        SectionDividerSetting value;
        value.type = *type;
        const std::optional<std::uint32_t> signature = reader.TryReadU32();
        if (signature == kBlockSignature)
            value.blend_key = reader.TryReadU32();
        return value;
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<SectionDividerSetting>(const SectionDividerSetting& value)
    {
        BigEndianWriter writer(4 * sizeof(std::uint32_t));
        writer.WriteU32(value.type);
        if (value.blend_key.has_value())
        {
            writer.WriteU32(kBlockSignature);
            writer.WriteU32(*value.blend_key);
            writer.WriteU32(kOrdinaryGroup);
        }
        return writer.Take();
    }
} // namespace ffpsd::detail
