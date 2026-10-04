#include "detail/color_mode_data.hpp"

namespace ffpsd::detail
{
    std::vector<std::uint8_t> ParseColorModeData(BigEndianReader& reader)
    {
        return reader.ReadBlob(reader.GetSize(), "color mode data");
    }

    void WriteColorModeData(BigEndianWriter& writer, const std::vector<std::uint8_t>& data)
    {
        writer.WriteBlob(data);
    }
} // namespace ffpsd::detail
