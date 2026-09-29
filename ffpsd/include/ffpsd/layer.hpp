#ifndef FFPSD_LAYER_HPP_
#define FFPSD_LAYER_HPP_

#include <cstddef>
#include <cstdint>
#include <ffpsd/adjustments.hpp>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <ffpsd/tagged_block.hpp>
#include <ffpsd/types.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ffpsd
{
    class Document;

    namespace detail
    {
        struct LayerRecord;
    } // namespace detail

    // A group takes two records; stored bottom to top, its end marker comes first.
    enum class LayerKind : std::uint8_t
    {
        kRaster,
        kGroupOpen,
        kGroupClosed,
        kGroupEnd,
        kAdjustment
    };

    // How Layer::Resize computes the new pixels.
    enum class ResampleFilter : std::uint8_t
    {
        kNearest, // the nearest source pixel: hard edges, no new values
        kBicubic  // Catmull-Rom; shrinking averages every source pixel under an output one
    };

    // Created by Document::AddLayer; not copyable or movable, so a pointer to it stays valid.
    class Layer
    {
    public:
        Layer() = delete;
        FFPSD_EXPORT ~Layer();

        Layer(const Layer& other) = delete;
        Layer& operator=(const Layer& other) = delete;
        Layer(Layer&& other) = delete;
        Layer& operator=(Layer&& other) = delete;

        // Groups from the 'lsct' block, adjustments from their own block; anything else is raster.
        FFPSD_EXPORT LayerKind GetKind() const noexcept;
        FFPSD_EXPORT Rect GetBounds() const noexcept;

        // The 'luni' name, or the legacy Pascal name when that block is missing.
        FFPSD_EXPORT std::string GetName() const;

        FFPSD_EXPORT std::uint8_t GetOpacity() const noexcept;
        FFPSD_EXPORT bool IsVisible() const noexcept;

        // Photoshop's background, marked by its 'lnsr' block; always the bottom layer.
        FFPSD_EXPORT bool IsBackground() const noexcept;

        // A raw fourcc such as 'norm', so an unknown mode survives a rewrite.
        FFPSD_EXPORT std::uint32_t GetBlendKey() const noexcept;

        FFPSD_EXPORT void SetOpacity(std::uint8_t opacity) noexcept;
        FFPSD_EXPORT void SetVisible(bool visible) noexcept;
        FFPSD_EXPORT void SetBlendKey(std::uint32_t blend_key) noexcept;

        // The block of an adjustment layer, such as 'levl', or 0; raw, so unknown ones are seen too.
        FFPSD_EXPORT std::uint32_t GetAdjustmentKey() const noexcept;

        // T is a struct from adjustments.hpp; null unless this layer is that adjustment.
        template <class T> FFPSD_EXPORT std::optional<T> GetAdjustment() const;

        // Only on a layer of T's kind; the composite goes stale.
        template <class T> FFPSD_EXPORT void SetAdjustment(const T& value);

        // Color planes by channel id, then transparency when the layer has one; decoded on each call.
        FFPSD_EXPORT Image GetPixels() const;

        // Replaces color and transparency at the same top left corner; masks stay, the composite goes stale.
        FFPSD_EXPORT void SetPixels(const Image& image);

        // Both need a raster layer without a mask; the background keeps 0, 0 and the canvas size.
        FFPSD_EXPORT void SetPosition(std::int32_t top, std::int32_t left);

        // Resamples the pixels, transparency included, keeping the top left corner.
        FFPSD_EXPORT void Resize(std::uint32_t width, std::uint32_t height, ResampleFilter filter = ResampleFilter::kBicubic);

#if defined(FFPSD_HAS_PNG)
        // GetPixels as a PNG, for gray and RGB documents; only with PNG, as png.hpp.
        FFPSD_EXPORT std::vector<std::uint8_t> SaveAsPng() const;
        FFPSD_EXPORT void SaveAsPng(const std::string& path) const;
#endif

        // Raw door to this layer's blocks, unchecked; pointers live until the block is removed.
        FFPSD_EXPORT std::size_t GetTaggedBlockCount() const noexcept;
        FFPSD_EXPORT const TaggedBlock* GetTaggedBlockByIndex(std::size_t index) const;

        // Null when there is no such key; keys may repeat, so this finds the first.
        FFPSD_EXPORT const TaggedBlock* GetTaggedBlockByKey(std::uint32_t key) const noexcept;

        // Assigns in place to the first block with this key, otherwise appends.
        FFPSD_EXPORT void SetTaggedBlock(const TaggedBlock& block);
        FFPSD_EXPORT bool RemoveTaggedBlock(std::uint32_t key);

    private:
        friend class Document;

        Layer(std::unique_ptr<detail::LayerRecord> record, Document* document) noexcept;

        // Throws for what SetPosition and Resize cannot handle; what names the call.
        void CheckTransformable(const char* what) const;

        std::unique_ptr<detail::LayerRecord> record_;
        Document* document_ = nullptr;
    };
} // namespace ffpsd

#endif // FFPSD_LAYER_HPP_
