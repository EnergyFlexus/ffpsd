#ifndef FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
#define FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fourcc.hpp"
#include "detail/layer_and_mask/channel_image_data.hpp"
#include "detail/layer_and_mask/tagged_block.hpp"

#include <cstddef>
#include <cstdint>
#include <ffpsd/image.hpp>
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

    // From the 'lyid' block; zero when the layer has none.
    std::uint32_t GetLayerId(const LayerRecord& record) noexcept;
    void SetLayerId(LayerRecord& record, std::uint32_t id);

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
    void WriteLayerRecord(
        BigEndianWriter& writer, const LayerRecord& record, const std::vector<const std::vector<std::uint8_t>*>& channels, bool is_psb);

    // Borrowed planar samples in native byte order; the caller keeps them alive for the call.
    struct SamplesView
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t channel_count = 0;
        std::uint16_t depth = 8;
    };

    // A view of the image's own bytes, valid while the image lives.
    SamplesView ViewOf(const Image& image) noexcept;

    // Channel -2 or -3 among the layer's channels.
    bool HasLayerMask(const LayerRecord& record) noexcept;

    // A layer may be no wider or taller than the document could be.
    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb);

    // Transparency first, then the color planes; no samples give empty channels.
    std::vector<ChannelImageData> EncodeLayerChannels(const SamplesView& samples, std::size_t color_count, bool is_psb);

    // The rectangle the samples cover at top and left; throws when it leaves the 32 bit range.
    Rect PlaceSamples(const SamplesView& samples, std::int32_t top, std::int32_t left);

    // The color planes by channel id, then transparency when the layer has it; masks stay out.
    Image DecodeLayerPixels(const LayerRecord& record, std::size_t color_count, std::uint16_t depth, bool is_psb);

    // New color and transparency channels at the same top left corner; the masks stay.
    void ReplaceLayerPixels(LayerRecord& record, const SamplesView& samples, std::size_t color_count, bool is_psb);

    // Color planes first; one plane more is transparency. No samples give empty channels.
    LayerRecord CreateLayerRecord(
        const std::string& name, const SamplesView& samples, std::int32_t top, std::int32_t left, std::size_t color_count, bool is_psb);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_LAYER_AND_MASK_LAYER_RECORD_HPP_
