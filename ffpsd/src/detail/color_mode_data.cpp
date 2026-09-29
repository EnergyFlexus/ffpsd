#include "detail/color_mode_data.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    std::vector<std::uint8_t> ParseColorModeData(BigEndianReader& reader)
    {
        const std::uint32_t length = reader.ReadU32();
        if (length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: color mode data claims " + std::to_string(length) + " bytes, only " + std::to_string(reader.GetRemaining()) +
                " left");

        std::vector<std::uint8_t> data(length);
        if (length != 0)
            reader.ReadU8Array(data.data(), length);

        return data;
    }

    void WriteColorModeData(BigEndianWriter& writer, const std::vector<std::uint8_t>& data)
    {
        const std::size_t length = writer.ReserveLength(false);
        if (!data.empty())
            writer.WriteU8Array(data.data(), data.size());
        writer.PatchLength(length, false);
    }
} // namespace ffpsd::detail
