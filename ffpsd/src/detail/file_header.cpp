#include "detail/file_header.hpp"

#include "detail/io/fourcc.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kSignature = Fourcc('8', 'B', 'P', 'S');
        constexpr std::size_t kReservedBytes = 6;
    } // namespace

    FileHeader ParseFileHeader(BigEndianReader& reader)
    {
        const std::uint32_t signature = reader.ReadU32();
        if (signature != kSignature)
            throw std::runtime_error("ffpsd: expected 8BPS signature, got '" + FourccString(signature) + "'");

        FileHeader header;
        header.version = reader.ReadU16();
        if (header.version != kVersionPsd && header.version != kVersionPsb)
            throw std::runtime_error("ffpsd: unsupported version " + std::to_string(header.version));

        reader.Skip(kReservedBytes);

        ImageInfo& info = header.info;
        info.channel_count = reader.ReadU16();
        info.height = reader.ReadU32();
        info.width = reader.ReadU32();
        info.depth = reader.ReadU16();
        info.color_mode = static_cast<ColorMode>(reader.ReadU16());

        CheckCanvasSides<std::runtime_error>(info.width, info.height, header.version == kVersionPsb);
        if (info.channel_count < 1 || info.channel_count > kMaxChannels)
            throw std::runtime_error("ffpsd: channel count out of range: " + std::to_string(info.channel_count));
        CheckDepthAndMode<std::runtime_error>(info.depth, info.color_mode);
        return header;
    }

    void WriteFileHeader(BigEndianWriter& writer, const FileHeader& header)
    {
        writer.WriteU32(kSignature);
        writer.WriteU16(header.version);
        writer.WriteZeros(kReservedBytes);
        writer.WriteU16(header.info.channel_count);
        writer.WriteU32(header.info.height);
        writer.WriteU32(header.info.width);
        writer.WriteU16(header.info.depth);
        writer.WriteU16(static_cast<std::uint16_t>(header.info.color_mode));
    }

    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width > max_side || height > max_side)
            throw std::invalid_argument(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " layer exceeds " + std::to_string(max_side) +
                " pixels a side");
    }
} // namespace ffpsd::detail
