#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_SECTION_DIVIDER_SETTING_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_SECTION_DIVIDER_SETTING_HPP_

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // 'lsct': the markers of a group, stored bottom to top, so its end comes first.
    struct SectionDividerSetting
    {
        static constexpr std::uint32_t kKey = Fourcc('l', 's', 'c', 't');
        static constexpr std::uint32_t kAnyOtherLayer = 0;
        static constexpr std::uint32_t kOpenFolder = 1;
        static constexpr std::uint32_t kClosedFolder = 2;
        static constexpr std::uint32_t kBoundingSectionDivider = 3;
        std::uint32_t type = kAnyOtherLayer;

        // A folder's blend mode, after an 8BIM signature; an end marker has none.
        std::optional<std::uint32_t> blend_key;
    };

    // Written with sub type 0, an ordinary group, so a scene group's sub type does not survive a rewrite.
    template <> std::optional<SectionDividerSetting> DecodeTaggedBlock<SectionDividerSetting>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeTaggedBlock<SectionDividerSetting>(const SectionDividerSetting& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_SECTION_DIVIDER_SETTING_HPP_
