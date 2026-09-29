#ifndef FFPSD_TAGGED_BLOCK_HPP_
#define FFPSD_TAGGED_BLOCK_HPP_

#include <cstdint>
#include <vector>

namespace ffpsd
{
    // Additional Layer Information; the data is raw, big endian.
    struct TaggedBlock
    {
        // '8BIM', or '8B64' for a block a PSB wrote with a 64 bit length.
        std::uint32_t signature = 0x3842494D;

        // Unknown keys are common and must survive a rewrite.
        std::uint32_t key = 0;

        std::vector<std::uint8_t> data;
    };
} // namespace ffpsd

#endif // FFPSD_TAGGED_BLOCK_HPP_
