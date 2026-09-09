#ifndef FFPSD_DETAIL_LAYER_AND_MASK_GLOBAL_LAYER_MASK_INFO_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_GLOBAL_LAYER_MASK_INFO_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    struct GlobalLayerMaskInfo
    {
        // Not interpreted, so kept whole.
        std::vector<std::uint8_t> raw;
    };

    // Its length field is 4 bytes in a PSB too.
    GlobalLayerMaskInfo ParseGlobalLayerMaskInfo(BigEndianReader& reader, std::size_t end);
    void WriteGlobalLayerMaskInfo(BigEndianWriter& writer, const GlobalLayerMaskInfo& info);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_GLOBAL_LAYER_MASK_INFO_HPP_
