#include "detail/image_resources/version_info.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/strings.hpp"

#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kBlockVersion = 1;
        constexpr std::size_t kFlagOffset = 4; // after the 4 byte version

        // Size of a block whose names are kProducerName.
        constexpr std::size_t kBlockReserve = 37;

        // Both name fields of a block ffpsd creates.
        constexpr char kProducerName[] = "ffpsd";

        // Zero when the block is too short to hold one.
        std::uint32_t BlockVersion(const std::vector<std::uint8_t>& data)
        {
            return BigEndianReader(data).TryReadU32().value_or(0);
        }

        // False when the length does not fit: nothing after the name can be located.
        bool ReadName(BigEndianReader& reader, std::string& text)
        {
            std::optional<std::string> name = TryReadUnicodeString(reader);
            if (!name.has_value())
                return false;
            text = std::move(*name);
            return true;
        }
    } // namespace

    template <> std::optional<VersionInfo> DecodeImageResource<VersionInfo>(const std::vector<std::uint8_t>& data)
    {
        if (data.size() <= kFlagOffset)
            return std::nullopt;

        VersionInfo info;
        BigEndianReader reader(data);
        info.version = reader.ReadU32();

        // The offsets below are known for version 1 only.
        if (info.version != kBlockVersion)
            return info;

        info.has_real_merged_data = reader.ReadU8() != 0;
        if (!ReadName(reader, info.writer_name) || !ReadName(reader, info.reader_name))
            return info;

        if (reader.GetRemaining() >= sizeof(std::uint32_t))
            info.file_version = reader.ReadU32();

        return info;
    }

    template <> std::vector<std::uint8_t> EncodeImageResource<VersionInfo>(const VersionInfo& info)
    {
        BigEndianWriter writer(kBlockReserve);
        writer.WriteU32(info.version);
        writer.WriteU8(info.has_real_merged_data ? 1 : 0);
        WriteUnicodeString(writer, info.writer_name);
        WriteUnicodeString(writer, info.reader_name);
        writer.WriteU32(info.file_version);

        return writer.Take();
    }

    bool HasRealMergedData(const ImageResources& image_resources)
    {
        // Reads the flag byte directly, without allocating for the names.
        const ImageResource* resource = FindImageResource(image_resources, VersionInfo::kId);
        if (resource == nullptr || resource->data.size() <= kFlagOffset || BlockVersion(resource->data) != kBlockVersion)
            return true;

        return resource->data[kFlagOffset] != 0;
    }

    void SetHasRealMergedData(ImageResources& image_resources, bool value)
    {
        ImageResource* resource = FindImageResource(image_resources, VersionInfo::kId);
        if (resource != nullptr && resource->data.size() > kFlagOffset && BlockVersion(resource->data) == kBlockVersion)
        {
            // One byte, so that the names of whoever wrote the file stay.
            resource->data[kFlagOffset] = value ? 1 : 0;
            return;
        }

        // No block, a truncated one, or an unknown version: write a whole one.
        VersionInfo info;
        info.has_real_merged_data = value;
        info.writer_name = kProducerName;
        info.reader_name = kProducerName;
        SetImageResource(image_resources, info);
    }
} // namespace ffpsd::detail
