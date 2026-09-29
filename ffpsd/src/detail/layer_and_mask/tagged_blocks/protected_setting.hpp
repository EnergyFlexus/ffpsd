#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_PROTECTED_SETTING_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_PROTECTED_SETTING_HPP_

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // 'lspf': bit 0 transparency, bit 1 composite and bit 2 position locked.
    struct ProtectedSetting
    {
        static constexpr std::uint32_t kKey = Fourcc('l', 's', 'p', 'f');
        std::uint32_t flags = 0;
    };

    template <> std::optional<ProtectedSetting> ParseTaggedBlock<ProtectedSetting>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeTaggedBlock<ProtectedSetting>(const ProtectedSetting& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_PROTECTED_SETTING_HPP_
