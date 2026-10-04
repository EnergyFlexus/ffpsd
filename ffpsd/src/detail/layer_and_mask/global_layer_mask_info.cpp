#include "detail/layer_and_mask/global_layer_mask_info.hpp"

namespace ffpsd::detail
{
    GlobalLayerMaskInfo ParseGlobalLayerMaskInfo(BigEndianReader& reader, std::size_t end)
    {
        GlobalLayerMaskInfo info;
        info.raw = reader.ReadBlob(end, "global layer mask info");
        return info;
    }

    void WriteGlobalLayerMaskInfo(BigEndianWriter& writer, const GlobalLayerMaskInfo& info)
    {
        writer.WriteBlob(info.raw);
    }
} // namespace ffpsd::detail
