#include "detail/image_resources/resolution_info.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fixed.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kBlockSize = 16;

        // Signed 16.16 cannot hold 32768.
        constexpr double kMaxResolution = 32767.0;
    } // namespace

    template <> std::optional<ResolutionInfo> DecodeImageResource<ResolutionInfo>(const std::vector<std::uint8_t>& data)
    {
        if (data.size() < kBlockSize)
            return std::nullopt;

        ResolutionInfo value;
        BigEndianReader reader(data);
        value.horizontal = FixedToDouble(reader.ReadU32());
        value.horizontal_unit = reader.ReadI16();
        value.width_unit = reader.ReadI16();
        value.vertical = FixedToDouble(reader.ReadU32());
        value.vertical_unit = reader.ReadI16();
        value.height_unit = reader.ReadI16();

        return value;
    }

    template <> std::vector<std::uint8_t> EncodeImageResource<ResolutionInfo>(const ResolutionInfo& value)
    {
        BigEndianWriter writer(kBlockSize);
        writer.WriteU32(DoubleToFixed(value.horizontal));
        writer.WriteI16(value.horizontal_unit);
        writer.WriteI16(value.width_unit);
        writer.WriteU32(DoubleToFixed(value.vertical));
        writer.WriteI16(value.vertical_unit);
        writer.WriteI16(value.height_unit);

        return writer.Take();
    }

    void CheckResolution(double dpi)
    {
        // Written as a negated comparison so that a NaN is rejected too.
        if (!(dpi > 0.0) || dpi > kMaxResolution)
            throw std::invalid_argument("ffpsd: resolution out of range: " + std::to_string(dpi));
    }
} // namespace ffpsd::detail
