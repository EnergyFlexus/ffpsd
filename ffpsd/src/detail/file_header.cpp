#include "detail/file_header.hpp"

#include "detail/io/fourcc.hpp"

#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kSignature = Fourcc('8', 'B', 'P', 'S');
        constexpr std::uint16_t kVersionPsd = 1;
        constexpr std::uint16_t kVersionPsb = 2;
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

        header.channel_count = reader.ReadU16();
        header.height = reader.ReadU32();
        header.width = reader.ReadU32();
        header.depth = reader.ReadU16();
        header.color_mode = reader.ReadU16();

        return header;
    }

    void WriteFileHeader(BigEndianWriter& writer, const FileHeader& header)
    {
        writer.WriteU32(kSignature);
        writer.WriteU16(header.version);
        writer.WriteZeros(kReservedBytes);
        writer.WriteU16(header.channel_count);
        writer.WriteU32(header.height);
        writer.WriteU32(header.width);
        writer.WriteU16(header.depth);
        writer.WriteU16(header.color_mode);
    }
} // namespace ffpsd::detail
