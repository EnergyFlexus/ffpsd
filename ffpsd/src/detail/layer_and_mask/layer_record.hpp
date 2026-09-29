#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/types.hpp>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // What ffpsd does not interpret stays as bytes.
    struct LayerRecord
    {
        Rect bounds;

        std::uint32_t blend_signature = Fourcc('8', 'B', 'I', 'M');
        std::uint32_t blend_key = Fourcc('n', 'o', 'r', 'm'); // 'mul ', 'scrn', ...
        std::uint8_t opacity = 255;
        std::uint8_t clipping = 0;

        // Bit 0 transparency locked, bit 1 hidden, bit 4 pixels irrelevant (valid when bit 3 is set).
        std::uint8_t flags = 0;

        // 0, 20, 36 or more bytes; the rectangle inside is the one channel -2 covers.
        std::vector<std::uint8_t> mask_data;

        std::vector<std::uint8_t> blending_ranges;

        // As stored; the real name is in the 'luni' block.
        std::string name;

        // In declared order, which is the order of the blobs in the file.
        std::vector<ChannelImageData> channels;

        TaggedBlocks blocks;
    };

    // Photoshop's background: transparency locked, 'lnsr' of 'bgnd' and position locked in 'lspf'.
    bool IsBackground(const LayerRecord& record) noexcept;
    void MarkAsBackground(LayerRecord& record);

    // What Photoshop gives a copy of its background: an ordinary layer again.
    void UnmarkBackground(LayerRecord& record);

    // A deep copy, blocks included; a new LayerRecord field has to be added here too.
    LayerRecord CopyLayerRecord(const LayerRecord& source);

    // Pass one of the layer info; channel_lengths gets the declared length of each channel.
    LayerRecord ParseLayerRecord(BigEndianReader& reader, bool is_psb, std::vector<std::uint64_t>& channel_lengths);

    // Pass one again, with the data written for each channel, which may differ from the record's.
    void WriteLayerRecord(BigEndianWriter& writer, const LayerRecord& record, const std::vector<const PixelData*>& channels, bool is_psb);

} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
