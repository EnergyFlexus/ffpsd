#include "detail/image_resources/image_resource.hpp"

#include "detail/file_header.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/io/strings.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
    } // namespace

    ImageResources ParseImageResources(BigEndianReader& reader)
    {
        const std::size_t section_length = reader.CheckLength(reader.ReadU32(), reader.GetSize(), "image resources section");

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
            entry.data = reader.ReadBytes(size);
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

    const ImageResource* FindImageResource(const ImageResources& image_resources, std::uint16_t id) noexcept
    {
        for (const std::unique_ptr<ImageResource>& resource : image_resources)
        {
            if (resource->id == id)
                return resource.get();
        }
        return nullptr;
    }
    ImageResource* FindImageResource(ImageResources& image_resources, std::uint16_t id) noexcept
    {
        const ImageResources& found_in = image_resources;
        return const_cast<ImageResource*>(FindImageResource(found_in, id));
    }

    ImageResource& FindOrInsertImageResource(ImageResources& image_resources, std::uint16_t id)
    {
        if (ImageResource* found = FindImageResource(image_resources, id))
            return *found;

        const auto at = std::find_if(
            image_resources.begin(), image_resources.end(), [id](const std::unique_ptr<ImageResource>& entry) { return entry->id > id; });

        auto created = std::make_unique<ImageResource>();
        created->id = id;
        return **image_resources.insert(at, std::move(created));
    }

    const ImageResource* ImageResourceAt(const ImageResources& image_resources, std::size_t index)
    {
        if (index >= image_resources.size())
            throw std::out_of_range(
                "ffpsd: image resource index " + std::to_string(index) + " of " + std::to_string(image_resources.size()));
        return image_resources[index].get();
    }

    bool RemoveImageResource(ImageResources& image_resources, std::uint16_t id)
    {
        const auto at = std::find_if(image_resources.begin(), image_resources.end(), [id](const std::unique_ptr<ImageResource>& resource) {
            return resource->id == id;
        });
        if (at == image_resources.end())
            return false;

        image_resources.erase(at);
        return true;
    }
} // namespace ffpsd::detail
