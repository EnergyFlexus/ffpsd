#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_NAME_SOURCE_SETTING_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_NAME_SOURCE_SETTING_HPP_

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // 'lnsr': where the name came from; 'bgnd' marks Photoshop's background.
    struct LayerNameSourceSetting
    {
        static constexpr std::uint32_t kKey = Fourcc('l', 'n', 's', 'r');
        static constexpr std::uint32_t kBackground = Fourcc('b', 'g', 'n', 'd');
        static constexpr std::uint32_t kLayer = Fourcc('l', 'a', 'y', 'r');
        std::uint32_t id = 0;
    };

    template <> std::optional<LayerNameSourceSetting> DecodeTaggedBlock<LayerNameSourceSetting>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerNameSourceSetting>(const LayerNameSourceSetting& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_NAME_SOURCE_SETTING_HPP_
