#ifndef FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCK_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCK_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/tagged_block.hpp>
#include <memory>
#include <vector>

namespace ffpsd::detail
{
    // One allocation per block, so pointers survive inserts and removals of others.
    using TaggedBlocks = std::vector<std::unique_ptr<TaggedBlock>>;

    // Payloads are padded to 4, not counted in the length; the spec's "even" is wrong.
    TaggedBlocks ParseTaggedBlocks(BigEndianReader& reader, std::size_t end, bool is_psb);
    void WriteTaggedBlock(BigEndianWriter& writer, const TaggedBlock& block, bool is_psb);
    void WriteTaggedBlocks(BigEndianWriter& writer, const TaggedBlocks& blocks, bool is_psb);

    TaggedBlocks CloneTaggedBlocks(const TaggedBlocks& blocks);

    const TaggedBlock* GetTaggedBlockByIndex(const TaggedBlocks& blocks, std::size_t index);

    // The first block with this key, or null.
    const TaggedBlock* GetTaggedBlockByKey(const TaggedBlocks& blocks, std::uint32_t key) noexcept;

    // The first block with this key, or a new empty one at the end.
    TaggedBlock& FindOrAppendTaggedBlock(TaggedBlocks& blocks, std::uint32_t key);

    // Removes the first block with this key.
    bool RemoveTaggedBlock(TaggedBlocks& blocks, std::uint32_t key);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_TAGGED_BLOCK_HPP_
