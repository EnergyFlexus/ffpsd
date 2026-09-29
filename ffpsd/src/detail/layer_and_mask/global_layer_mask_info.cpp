#include "detail/layer_and_mask/global_layer_mask_info.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    GlobalLayerMaskInfo ParseGlobalLayerMaskInfo(BigEndianReader& reader, std::size_t end)
    {
        const std::uint32_t length = reader.ReadU32();
        if (reader.Tell() > end || length > end - reader.Tell())
            throw std::runtime_error(
                "ffpsd: global layer mask info claims " + std::to_string(length) + " bytes, more than the section holds");

        GlobalLayerMaskInfo info;
        info.raw.resize(length);
        if (length != 0)
            reader.ReadU8Array(info.raw.data(), info.raw.size());
        return info;
    }

    void WriteGlobalLayerMaskInfo(BigEndianWriter& writer, const GlobalLayerMaskInfo& info)
    {
        const std::size_t length = writer.ReserveLength(false);
        if (!info.raw.empty())
            writer.WriteU8Array(info.raw.data(), info.raw.size());
        writer.PatchLength(length, false);
    }
} // namespace ffpsd::detail
