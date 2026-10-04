#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_TAGGED_BLOCK_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_TAGGED_BLOCK_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/tagged_block.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    // One allocation per block, so pointers survive inserts and removals of others.
    using TaggedBlocks = std::vector<std::unique_ptr<TaggedBlock>>;

    // Padding to 4 after a length is skipped, unless the next block starts right after the data.
    TaggedBlocks ParseTaggedBlocks(BigEndianReader& reader, std::size_t end, bool is_psb);
    // Section blocks, padded to 4 after the length as Photoshop writes them.
    void WriteSectionTaggedBlock(BigEndianWriter& writer, const TaggedBlock& block, bool is_psb);

    // Layer blocks: Photoshop counts their padding in the length and rejects them otherwise.
    void WriteLayerTaggedBlocks(BigEndianWriter& writer, const TaggedBlocks& blocks, bool is_psb);

    TaggedBlocks CopyTaggedBlocks(const TaggedBlocks& blocks);

    const TaggedBlock* TaggedBlockAt(const TaggedBlocks& blocks, std::size_t index);

    // The first block with this key, or null.
    const TaggedBlock* FindTaggedBlock(const TaggedBlocks& blocks, std::uint32_t key) noexcept;

    // The first block with this key, or a new empty one at the end.
    TaggedBlock& FindOrAppendTaggedBlock(TaggedBlocks& blocks, std::uint32_t key);

    // Removes the first block with this key.
    bool RemoveTaggedBlock(TaggedBlocks& blocks, std::uint32_t key);

    // Specialized by each block ffpsd interprets, for its struct with kKey; empty when the data cannot be read.
    template <class T> std::optional<T> DecodeTaggedBlock(const std::vector<std::uint8_t>& data);
    template <class T> std::vector<std::uint8_t> EncodeTaggedBlock(const T& value);

    // The first block with T's key, read; empty when there is none or it cannot be read.
    template <class T> std::optional<T> GetTaggedBlock(const TaggedBlocks& blocks)
    {
        const TaggedBlock* block = FindTaggedBlock(blocks, T::kKey);
        if (block == nullptr)
            return std::nullopt;
        return DecodeTaggedBlock<T>(block->data);
    }

    // Assigns in place to the first block with T's key, otherwise appends.
    template <class T> void SetTaggedBlock(TaggedBlocks& blocks, const T& value)
    {
        std::vector<std::uint8_t> data = EncodeTaggedBlock(value);
        FindOrAppendTaggedBlock(blocks, T::kKey).data = std::move(data);
    }

} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCKS_TAGGED_BLOCK_HPP_
