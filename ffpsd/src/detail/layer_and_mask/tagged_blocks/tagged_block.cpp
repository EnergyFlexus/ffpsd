#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include "detail/io/fourcc.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kSignature = Fourcc('8', 'B', 'I', 'M');
        constexpr std::uint32_t kSignature64 = Fourcc('8', 'B', '6', '4');

        // A signature, a key and a 4 byte length.
        constexpr std::size_t kMinBlockSize = 12;

        constexpr std::size_t kAlignment = 4;

        // In a PSB these keys have an 8 byte length; nothing but this list says so.
        constexpr std::uint32_t kLongLengthKeys[] = {Fourcc('L', 'M', 's', 'k'), Fourcc('L', 'r', '1', '6'), Fourcc('L', 'r', '3', '2'),
                                                     Fourcc('L', 'a', 'y', 'r'), Fourcc('M', 't', '1', '6'), Fourcc('M', 't', '3', '2'),
                                                     Fourcc('M', 't', 'r', 'n'), Fourcc('A', 'l', 'p', 'h'), Fourcc('F', 'M', 's', 'k'),
                                                     Fourcc('l', 'n', 'k', '2'), Fourcc('F', 'E', 'i', 'd'), Fourcc('F', 'X', 'i', 'd'),
                                                     Fourcc('P', 'x', 'S', 'D')};

        bool StartsBlock(const BigEndianReader& reader, std::size_t end)
        {
            if (end - reader.Tell() < sizeof(std::uint32_t))
                return false;
            const std::uint32_t signature = reader.PeekU32();
            return signature == kSignature || signature == kSignature64;
        }

        bool UsesLongLength(std::uint32_t key) noexcept
        {
            const auto* at = std::find(std::begin(kLongLengthKeys), std::end(kLongLengthKeys), key);
            return at != std::end(kLongLengthKeys);
        }

        void WriteBlock(BigEndianWriter& writer, const TaggedBlock& block, bool is_psb, bool pad_in_length)
        {
            writer.WriteU32(block.signature);
            writer.WriteU32(block.key);

            const bool wide = is_psb && UsesLongLength(block.key);
            const std::size_t length = writer.ReserveLength(wide);
            const std::size_t data_start = writer.Tell();
            if (!block.data.empty())
                writer.WriteU8Array(block.data.data(), block.data.size());

            if (pad_in_length)
            {
                writer.PadFrom(data_start, kAlignment);
                writer.PatchLength(length, wide);
            }
            else
            {
                writer.PatchLength(length, wide);
                writer.PadFrom(data_start, kAlignment);
            }
        }
    } // namespace

    TaggedBlocks ParseTaggedBlocks(BigEndianReader& reader, std::size_t end, bool is_psb)
    {
        if (end < reader.Tell() || end > reader.GetSize())
            throw std::out_of_range("ffpsd: tagged block end offset " + std::to_string(end) + " outside the file");

        TaggedBlocks blocks;

        // Fewer bytes left than a header needs is padding, not a block.
        while (reader.Tell() + kMinBlockSize <= end)
        {
            const std::size_t block_start = reader.Tell();

            auto block = std::make_unique<TaggedBlock>();
            block->signature = reader.ReadU32();
            if (block->signature != kSignature && block->signature != kSignature64)
                throw std::runtime_error(
                    "ffpsd: expected 8BIM or 8B64 at offset " + std::to_string(block_start) + ", got '" + FourccString(block->signature) +
                    "'");

            block->key = reader.ReadU32();

            const std::uint64_t length = is_psb && UsesLongLength(block->key) ? reader.ReadU64() : reader.ReadU32();

            // The reader sees past the section end, so check the length field here.
            if (reader.Tell() > end)
                throw std::runtime_error(
                    "ffpsd: tagged block at offset " + std::to_string(block_start) +
                    " has a length field that runs past the end of the section");

            const std::size_t left = end - reader.Tell();
            if (length > left)
                throw std::runtime_error(
                    "ffpsd: tagged block '" + FourccString(block->key) + "' at offset " + std::to_string(block_start) + " claims " +
                    std::to_string(length) + " bytes, only " + std::to_string(left) + " left in the section");

            block->data.resize(static_cast<std::size_t>(length));
            if (!block->data.empty())
                reader.ReadU8Array(block->data.data(), block->data.size());

            // Padding to 4 unless a block starts here (then it was 2, inside the length); the last may lack it.
            const std::size_t pad = (kAlignment - block->data.size() % kAlignment) % kAlignment;
            if (pad != 0 && !StartsBlock(reader, end))
                reader.Skip(std::min(pad, end - reader.Tell()));

            blocks.push_back(std::move(block));
        }

        reader.Skip(end - reader.Tell());
        return blocks;
    }

    void WriteTaggedBlock(BigEndianWriter& writer, const TaggedBlock& block, bool is_psb)
    {
        WriteBlock(writer, block, is_psb, false);
    }

    void WriteLayerTaggedBlocks(BigEndianWriter& writer, const TaggedBlocks& blocks, bool is_psb)
    {
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
            WriteBlock(writer, *block, is_psb, true);
    }

    TaggedBlocks CloneTaggedBlocks(const TaggedBlocks& blocks)
    {
        TaggedBlocks copy;
        copy.reserve(blocks.size());
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
            copy.push_back(std::make_unique<TaggedBlock>(*block));
        return copy;
    }

    const TaggedBlock* GetTaggedBlockByIndex(const TaggedBlocks& blocks, std::size_t index)
    {
        if (index >= blocks.size())
            throw std::out_of_range("ffpsd: tagged block index " + std::to_string(index) + " of " + std::to_string(blocks.size()));

        return blocks[index].get();
    }

    const TaggedBlock* GetTaggedBlockByKey(const TaggedBlocks& blocks, std::uint32_t key) noexcept
    {
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
        {
            if (block->key == key)
                return block.get();
        }
        return nullptr;
    }

    TaggedBlock& FindOrAppendTaggedBlock(TaggedBlocks& blocks, std::uint32_t key)
    {
        for (const std::unique_ptr<TaggedBlock>& block : blocks)
        {
            if (block->key == key)
                return *block;
        }

        auto created = std::make_unique<TaggedBlock>();
        created->key = key;
        blocks.push_back(std::move(created));
        return *blocks.back();
    }

    bool RemoveTaggedBlock(TaggedBlocks& blocks, std::uint32_t key)
    {
        const auto at =
            std::find_if(blocks.begin(), blocks.end(), [key](const std::unique_ptr<TaggedBlock>& block) { return block->key == key; });
        if (at == blocks.end())
            return false;

        blocks.erase(at);
        return true;
    }

    std::optional<std::uint32_t> ParseU32Block(const std::vector<std::uint8_t>& data)
    {
        if (data.size() < sizeof(std::uint32_t))
            return std::nullopt;
        BigEndianReader reader(data);
        return reader.ReadU32();
    }

    std::vector<std::uint8_t> EncodeU32Block(std::uint32_t value)
    {
        BigEndianWriter writer(sizeof(std::uint32_t));
        writer.WriteU32(value);
        return writer.Take();
    }
} // namespace ffpsd::detail
