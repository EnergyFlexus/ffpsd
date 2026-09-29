#include "detail/image_resources/image_resource.hpp"

#include "detail/io/fourcc.hpp"
#include "detail/io/strings.hpp"

#include <algorithm>
#include <stdexcept>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kBlockSignature = Fourcc('8', 'B', 'I', 'M');
    } // namespace

    ImageResources ParseImageResources(BigEndianReader& reader)
    {
        const std::uint32_t section_length = reader.ReadU32();
        if (section_length > reader.GetRemaining())
            throw std::runtime_error(
                "ffpsd: image resources section claims " + std::to_string(section_length) + " bytes, only " +
                std::to_string(reader.GetRemaining()) + " left");

        const std::size_t section_end = reader.Tell() + section_length;

        ImageResources image_resources;
        while (reader.Tell() < section_end)
        {
            const std::size_t block_start = reader.Tell();

            const std::uint32_t signature = reader.ReadU32();
            if (signature != kBlockSignature)
                throw std::runtime_error(
                    "ffpsd: expected 8BIM at offset " + std::to_string(block_start) + ", got '" + FourccString(signature) + "'");

            ImageResource entry;
            entry.id = reader.ReadU16();
            entry.name = ReadPascalString(reader, kImageResourceNameAlignment);

            const std::uint32_t size = reader.ReadU32();
            entry.data.resize(size);
            if (size != 0)
                reader.ReadU8Array(entry.data.data(), size);
            if (size % 2 != 0)
                reader.Skip(1); // the pad byte is not counted in size

            if (reader.Tell() > section_end)
                throw std::runtime_error(
                    "ffpsd: image resource " + std::to_string(entry.id) + " at offset " + std::to_string(block_start) +
                    " runs past the end of the section");

            image_resources.push_back(std::make_unique<ImageResource>(std::move(entry)));
        }

        return image_resources;
    }

    void WriteImageResources(BigEndianWriter& writer, const ImageResources& image_resources)
    {
        const std::size_t section_length = writer.ReserveLength(false);
        for (const std::unique_ptr<ImageResource>& entry : image_resources)
        {
            writer.WriteU32(kBlockSignature);
            writer.WriteU16(entry->id);
            WritePascalString(writer, entry->name, kImageResourceNameAlignment);

            const std::size_t size = writer.ReserveLength(false);
            if (!entry->data.empty())
                writer.WriteU8Array(entry->data.data(), entry->data.size());
            writer.PatchLength(size, false);
            writer.PadFrom(size, 2);
        }
        writer.PatchLength(section_length, false);
    }

    std::size_t FindImageResourceIndex(const ImageResources& image_resources, std::uint16_t id)
    {
        for (std::size_t i = 0; i < image_resources.size(); ++i)
        {
            if (image_resources[i]->id == id)
                return i;
        }
        return kNoImageResource;
    }

    ImageResource& FindOrInsertImageResource(ImageResources& image_resources, std::uint16_t id)
    {
        const std::size_t found = FindImageResourceIndex(image_resources, id);
        if (found != kNoImageResource)
            return *image_resources[found];

        const auto at = std::find_if(
            image_resources.begin(), image_resources.end(), [id](const std::unique_ptr<ImageResource>& entry) { return entry->id > id; });

        auto created = std::make_unique<ImageResource>();
        created->id = id;
        return **image_resources.insert(at, std::move(created));
    }
} // namespace ffpsd::detail
