#include "detail/layer_and_mask/tagged_blocks/layer_id.hpp"

namespace ffpsd::detail
{
    template <> std::optional<LayerId> ParseTaggedBlock<LayerId>(const std::vector<std::uint8_t>& data)
    {
        const std::optional<std::uint32_t> value = ParseU32Block(data);
        if (!value.has_value())
            return std::nullopt;
        return LayerId{*value};
    }

    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerId>(const LayerId& value)
    {
        return EncodeU32Block(value.id);
    }
} // namespace ffpsd::detail
