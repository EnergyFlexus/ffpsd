#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/mask_data.hpp"
#include "detail/layer_and_mask/tagged_blocks/tagged_block.hpp"
#include "detail/signatures.hpp"

#include <cstdint>
#include <ffpsd/types.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // What ffpsd does not interpret stays as bytes.
    struct LayerRecord
    {
        // Copied only through Copy: a record holds every channel's pixels.
        LayerRecord() = default;
        LayerRecord(const LayerRecord&) = delete;
        LayerRecord& operator=(const LayerRecord&) = delete;
        LayerRecord(LayerRecord&&) noexcept = default;
        LayerRecord& operator=(LayerRecord&&) noexcept = default;
        ~LayerRecord() = default;

        Rect bounds;

        std::uint32_t blend_signature = kBlockSignature;
        std::uint32_t blend_key = Fourcc('n', 'o', 'r', 'm'); // 'mul ', 'scrn', ...
        std::uint8_t opacity = 255;
        std::uint8_t clipping = 0;

        // Bit 0 transparency locked, bit 1 hidden, bit 4 pixels irrelevant (valid when bit 3 is set).
        std::uint8_t flags = 0;

        // 0, 20, 36 or more bytes; the rectangle inside is the one channel -2 covers.
        MaskData mask_data;

        std::vector<std::uint8_t> blending_ranges;

        // As stored; the real name is in the 'luni' block.
        std::string name;

        // In declared order, which is the order of the blobs in the file.
        std::vector<ChannelImageData> channels;

        TaggedBlocks blocks;

        // No channels, at top and left, with the flags and blending ranges Photoshop gives a new layer.
        static LayerRecord Empty(const std::string& name, std::int32_t top, std::int32_t left);

        // A deep copy, blocks included; a new field has to be added here too.
        LayerRecord Copy() const;

        // Null when the record has no channel of that id.
        const ChannelImageData* FindChannel(std::int16_t id) const noexcept;
        void RemoveChannels(std::int16_t id);

        // Channel -2 or -3 among the channels.
        bool HasLayerMask() const noexcept;
        bool HasVectorMask() const noexcept;

        // The channel of the pixel mask; a -2 rendered from a vector mask is not one.
        std::optional<std::int16_t> FindPixelMaskId() const noexcept;

        // The 'luni' block and the legacy Pascal copy, which is cut to 255 bytes on writing.
        void SetName(const std::string& new_name);

        // Any one makes Photoshop take a bottom raster layer for the background: locked transparency, an 'lspf' lock, no -1.
        bool HasBackgroundMarks() const noexcept;

        // Marked: transparency locked, 'lnsr' of 'bgnd' and position locked in 'lspf', as Photoshop marks it.
        void MarkAsBackground();

        // What Photoshop gives a copy of its background: an ordinary layer again, with opaque transparency when it had none.
        void UnmarkBackground(std::uint16_t depth, bool is_psb);
    };

    // Pass one of the layer info; channel_lengths gets the declared length of each channel.
    LayerRecord ParseLayerRecord(BigEndianReader& reader, bool is_psb, std::vector<std::uint64_t>& channel_lengths);

    // Pass one again, with the data written for each channel, which may differ from the record's.
    void WriteLayerRecord(BigEndianWriter& writer, const LayerRecord& record, const std::vector<const PixelData*>& channels, bool is_psb);

} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
