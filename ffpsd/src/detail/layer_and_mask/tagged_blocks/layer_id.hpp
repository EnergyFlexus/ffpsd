#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_ID_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_ID_HPP_

#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ffpsd::detail
{
    // 'lyid': unique in the document; resource 1044 keeps the last one handed out.
    struct LayerId
    {
        static constexpr std::uint32_t kKey = Fourcc('l', 'y', 'i', 'd');
        std::uint32_t id = 0;
    };

    template <> std::optional<LayerId> ParseTaggedBlock<LayerId>(const std::vector<std::uint8_t>& data);
    template <> std::vector<std::uint8_t> EncodeTaggedBlock<LayerId>(const LayerId& value);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_LAYER_ID_HPP_
