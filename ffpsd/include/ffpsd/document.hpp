#ifndef FFPSD_DOCUMENT_HPP_
#define FFPSD_DOCUMENT_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <ffpsd/image_resources.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/tagged_block.hpp>
#include <ffpsd/types.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ffpsd
{
    class Document
    {
    public:
        // Paths are UTF-8 on every system, here and in formats.hpp.
        FFPSD_EXPORT static Document Open(const std::string& path);
        FFPSD_EXPORT static Document Parse(const std::vector<std::uint8_t>& data);
        FFPSD_EXPORT static Document Parse(const std::uint8_t* data, std::size_t size);

        // Unknown blocks, masks and ZIP data are written as read; without a composite, section 5 gets zeros.
        FFPSD_EXPORT void Save(const std::string& path, Compression compression = Compression::kDefault) const;
        FFPSD_EXPORT std::vector<std::uint8_t> Save(Compression compression = Compression::kDefault) const;

        // Multichannel throws: it has no fixed channel count. A side over 30000 makes it a PSB.
        FFPSD_EXPORT Document(std::uint32_t width, std::uint32_t height, ColorMode color_mode, std::uint16_t depth = 8);
        FFPSD_EXPORT ~Document();
        FFPSD_EXPORT Document(Document&& other) noexcept;
        FFPSD_EXPORT Document& operator=(Document&& other) noexcept;

        Document() = delete;
        Document(const Document& other) = delete;
        Document& operator=(const Document& other) = delete;

        FFPSD_EXPORT std::uint32_t GetWidth() const noexcept;
        FFPSD_EXPORT std::uint32_t GetHeight() const noexcept;
        FFPSD_EXPORT std::uint16_t GetChannelCount() const noexcept;
        FFPSD_EXPORT std::uint16_t GetDepth() const noexcept;
        FFPSD_EXPORT ColorMode GetColorMode() const noexcept;

        // PSB: the large document format, with wider lengths.
        FFPSD_EXPORT bool IsPsb() const noexcept;

        // False: the composite in section 5 is a placeholder, build the picture from the layers.
        FFPSD_EXPORT bool HasRealMergedData() const noexcept;

        // Resource 1057 in full; allocates for the names.
        FFPSD_EXPORT VersionInfo GetVersionInfo() const;

        // 72 dpi when the file omits resource 1005.
        FFPSD_EXPORT ResolutionInfo GetResolutionInfo() const noexcept;

        // The background and composite are cut or padded with white; text, smart objects, shapes and vector masks throw.
        FFPSD_EXPORT void ResizeCanvas(std::uint32_t width, std::uint32_t height, Anchor anchor = Anchor::kCenter);

        // Text, smart objects and shapes throw; layer effects keep their sizes.
        FFPSD_EXPORT void Resize(std::uint32_t width, std::uint32_t height, ResampleFilter filter = ResampleFilter::kBicubic);

        // Every layer, mask and the composite, exactly; a quarter turn swaps the sides and the resolutions.
        FFPSD_EXPORT void FlipCanvas(FlipDirection direction);
        FFPSD_EXPORT void RotateCanvas(Rotation rotation);

        // Every layer and the composite, RGB to gray and back; any other pair throws std::invalid_argument.
        FFPSD_EXPORT void ConvertColorMode(ColorMode color_mode);

        // Rewrites the RLE row counts for the other format; throws std::logic_error for an RLE layer mask.
        FFPSD_EXPORT void SetPsb(bool psb);

        // Patches one byte of resource 1057, keeping its names; creates the block if missing.
        FFPSD_EXPORT void SetHasRealMergedData(bool value);

        // Replace the whole resource block, creating it when there is none.
        FFPSD_EXPORT void SetVersionInfo(VersionInfo value);
        FFPSD_EXPORT void SetResolutionInfo(ResolutionInfo value);

        // Pointers live until the resource is removed and show its current contents.
        FFPSD_EXPORT std::size_t GetImageResourceCount() const noexcept;
        FFPSD_EXPORT const ImageResource* GetImageResourceByIndex(std::size_t index) const;

        // Null when there is no such id; ids may repeat, so this finds the first.
        FFPSD_EXPORT const ImageResource* GetImageResourceById(std::uint16_t id) const noexcept;

        // Assigns in place, so pointers stay valid; a new id is inserted in id order.
        FFPSD_EXPORT void SetImageResource(const ImageResource& resource);

        FFPSD_EXPORT bool RemoveImageResource(std::uint16_t id);

        // Bottom to top; a stack change drops resources 1024, 1026, 1072 and the real composite flag.
        FFPSD_EXPORT std::size_t GetLayerCount() const noexcept;
        FFPSD_EXPORT Layer* GetLayerByIndex(std::size_t index);
        FFPSD_EXPORT const Layer* GetLayerByIndex(std::size_t index) const;

        // Adds a raster layer on top. One image plane beyond the color channels is transparency.
        FFPSD_EXPORT Layer*
        AddLayer(const std::string& name, const ImageView& image = ImageView(), std::int32_t top = 0, std::int32_t left = 0);

        // Photoshop's locked background: at the bottom, the document's size, one at most; alpha goes onto white.
        FFPSD_EXPORT Layer* AddBackgroundLayer(const std::string& name, const ImageView& image);

        // A raster layer without a mask becomes the background: over white, fitted to the canvas, moved down.
        FFPSD_EXPORT void SetBackgroundLayer(std::size_t index);

        // Makes the background an ordinary layer where it is, unlocked; false when there is none.
        FFPSD_EXPORT bool UnsetBackgroundLayer();

        // An adjustment layer on top; T is a struct from adjustments.hpp.
        template <class T> FFPSD_EXPORT Layer* AddAdjustmentLayer(const std::string& name, const T& value = T());

        // A copy on top, from a document of the same format; a copy of the background is an ordinary layer.
        FFPSD_EXPORT Layer* AddLayerCopy(const Layer& source);

        // Destroys the layer, so pointers to it dangle.
        FFPSD_EXPORT void RemoveLayer(std::size_t index);

        // Afterwards the layer is at index to; pointers stay valid. The background does not move.
        FFPSD_EXPORT void MoveLayer(std::size_t from, std::size_t to);

        // Section 4 blocks after the layers, unchecked; the same rules as a layer's blocks.
        FFPSD_EXPORT std::size_t GetTaggedBlockCount() const noexcept;
        FFPSD_EXPORT const TaggedBlock* GetTaggedBlockByIndex(std::size_t index) const;
        FFPSD_EXPORT const TaggedBlock* GetTaggedBlockByKey(std::uint32_t key) const noexcept;
        FFPSD_EXPORT void SetTaggedBlock(const TaggedBlock& block);
        FFPSD_EXPORT bool RemoveTaggedBlock(std::uint32_t key);

        // Section 5, decoded on each call; an empty Image when the file has none.
        FFPSD_EXPORT Image GetMergedImage() const;

        // Its shape without decoding, then its bytes into the caller's memory, exactly GetSizeBytes of it.
        FFPSD_EXPORT ImageInfo GetMergedImageInfo() const;
        FFPSD_EXPORT void GetMergedImageBytes(std::uint8_t* out, std::size_t size) const;

        FFPSD_EXPORT std::optional<ChannelCompression> GetMergedCompression() const noexcept;

        // Channels past the colors are alpha or spot channels and set the channel count; sets has_real_merged_data.
        FFPSD_EXPORT void SetMergedImage(const ImageView& image);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        explicit Document(std::unique_ptr<Impl> impl) noexcept;

        // Repoints the layers at this document after a move.
        void RebindLayers() noexcept;

        void MarkStackChanged();

        void DropMergedImage();

        void Apply(
            const detail::Transform& transform, std::uint32_t width, std::uint32_t height,
            ResampleFilter filter = ResampleFilter::kNearest);

        // The next id after resource 1044 and every layer's, written to both.
        void AssignLayerId(detail::LayerRecord& record);
    };
} // namespace ffpsd

#endif // FFPSD_DOCUMENT_HPP_
