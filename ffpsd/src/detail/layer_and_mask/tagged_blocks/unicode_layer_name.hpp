#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_UNICODE_LAYER_NAME_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_UNICODE_LAYER_NAME_HPP_

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // 'luni': the full name; the Pascal name in the record is a legacy copy of it.
    struct UnicodeLayerName
    {
        static constexpr std::uint32_t kKey = Fourcc('l', 'u', 'n', 'i');
        std::string name;
    };

    // Empty when the count is more than the block holds.
    template <> std::optional<UnicodeLayerName> ParseTaggedBlock<UnicodeLayerName>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeTaggedBlock<UnicodeLayerName>(const UnicodeLayerName& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_UNICODE_LAYER_NAME_HPP_
