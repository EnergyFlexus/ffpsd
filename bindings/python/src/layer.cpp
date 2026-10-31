#include "common.hpp"

#include <functional>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <vector>

namespace ffpsd_python
{
    using namespace nb::literals;

    namespace
    {
        struct MaskObject
        {
            NumpyArray image;
            ffpsd::Rect bounds;
            std::uint8_t default_color = 255;
        };

        template <class T> std::optional<T> Copy(const T* found)
        {
            return found ? std::optional<T>(*found) : std::nullopt;
        }
    } // namespace

    void BindLayer(nb::module_& m)
    {
        nb::class_<MaskObject>(m, "LayerMask")
            // The array owns its pixels already, so it cannot also borrow from the mask.
            .def_prop_ro(
                "image", [](const MaskObject& mask) { return mask.image; }, nb::rv_policy::automatic)
            .def_ro("bounds", &MaskObject::bounds)
            .def_ro("default_color", &MaskObject::default_color)
            .def("__repr__", [](const MaskObject& mask) {
                return "LayerMask(bounds=" + nb::cast<std::string>(nb::repr(nb::cast(mask.bounds))) +
                       ", default_color=" + std::to_string(mask.default_color) + ")";
            });

        nb::class_<LayerObject>(m, "Layer")
            .def_prop_ro("document", [](const LayerObject& self) { return self.owner; })
            .def_prop_ro("index", [](const LayerObject& self) { return *self.document->document.FindLayerIndex(self.Get()); })
            .def_prop_ro("kind", [](const LayerObject& self) { return self.Get().GetKind(); })
            .def_prop_ro("bounds", [](const LayerObject& self) { return self.Get().GetBounds(); })
            .def_prop_ro("is_background", [](const LayerObject& self) { return self.Get().IsBackground(); })
            .def_prop_rw(
                "name", [](const LayerObject& self) { return self.Get().GetName(); },
                [](LayerObject& self, const std::string& name) { self.Get().SetName(name); })
            .def_prop_rw(
                "opacity", [](const LayerObject& self) { return self.Get().GetOpacity(); },
                [](LayerObject& self, std::uint8_t opacity) { self.Get().SetOpacity(opacity); })
            .def_prop_rw(
                "visible", [](const LayerObject& self) { return self.Get().IsVisible(); },
                [](LayerObject& self, bool visible) { self.Get().SetVisible(visible); })
            .def_prop_rw(
                "clipped", [](const LayerObject& self) { return self.Get().IsClipped(); },
                [](LayerObject& self, bool clipped) { self.Get().SetClipped(clipped); })
            .def_prop_rw(
                "group_open", [](const LayerObject& self) { return self.Get().GetKind() == ffpsd::LayerKind::kGroupOpen; },
                [](LayerObject& self, bool open) { self.Get().SetGroupOpen(open); })
            .def_prop_rw(
                "blend_key", [](const LayerObject& self) { return self.Get().GetBlendKey(); },
                [](LayerObject& self, std::uint32_t key) { self.Get().SetBlendKey(key); })
            .def_prop_ro(
                "adjustment_key",
                [](const LayerObject& self) -> std::optional<std::uint32_t> {
                    const std::uint32_t key = self.Get().GetAdjustmentKey();
                    return key == 0 ? std::nullopt : std::optional<std::uint32_t>(key);
                })
            .def_prop_rw(
                "levels", [](const LayerObject& self) { return self.Get().GetAdjustment<ffpsd::LevelsInfo>(); },
                [](LayerObject& self, const ffpsd::LevelsInfo& levels) { self.Get().SetAdjustment(levels); })

            .def("pixels", [](const LayerObject& self) { return ToArray(self.Get().GetPixels()); })
            .def("pixels_info", [](const LayerObject& self) { return self.Get().GetPixelsInfo(); })
            .def(
                "set_pixels",
                [](LayerObject& self, const PixelArray& pixels) {
                    self.Get().SetPixels(ToView(pixels, self.document->document.GetColorMode()));
                },
                "pixels"_a)
            .def(
                "set_position", [](LayerObject& self, std::int32_t top, std::int32_t left) { self.Get().SetPosition(top, left); }, "top"_a,
                "left"_a)
            .def(
                "resize",
                [](LayerObject& self, std::uint32_t width, std::uint32_t height, ffpsd::ResampleFilter filter) {
                    self.Get().Resize(width, height, filter);
                },
                "width"_a, "height"_a, "filter"_a = ffpsd::ResampleFilter::kBicubic)
            .def(
                "flip", [](LayerObject& self, ffpsd::FlipDirection direction) { self.Get().Flip(direction); }, "direction"_a)
            .def(
                "rotate", [](LayerObject& self, ffpsd::Rotation rotation) { self.Get().Rotate(rotation); }, "rotation"_a)

            .def(
                "mask",
                [](const LayerObject& self) -> std::optional<MaskObject> {
                    std::optional<ffpsd::LayerMask> mask = self.Get().GetMask();
                    if (!mask)
                        return std::nullopt;
                    return MaskObject{ToArray(std::move(mask->image)), mask->bounds, mask->default_color};
                })
            .def(
                "set_mask",
                [](LayerObject& self, const PixelArray& pixels, std::int32_t top, std::int32_t left, std::uint8_t default_color) {
                    self.Get().SetMask(ToView(pixels, ffpsd::ColorMode::kGrayscale), top, left, default_color);
                },
                "pixels"_a, "top"_a = 0, "left"_a = 0, "default_color"_a = 255)
            .def("remove_mask", [](LayerObject& self) { return self.Get().RemoveMask(); })
            .def_prop_ro(
                "channels",
                [](const LayerObject& self) {
                    const ffpsd::Layer& layer = self.Get();
                    std::vector<ffpsd::ChannelInfo> channels;
                    for (std::size_t i = 0; i < layer.GetChannelCount(); ++i)
                        channels.push_back(layer.GetChannelByIndex(i));
                    return channels;
                })

            .def_prop_ro(
                "tagged_blocks",
                [](const LayerObject& self) {
                    const ffpsd::Layer& layer = self.Get();
                    std::vector<ffpsd::TaggedBlock> blocks;
                    for (std::size_t i = 0; i < layer.GetTaggedBlockCount(); ++i)
                        blocks.push_back(*layer.GetTaggedBlockByIndex(i));
                    return blocks;
                })
            .def(
                "find_tagged_block", [](const LayerObject& self, std::uint32_t key) { return Copy(self.Get().GetTaggedBlockByKey(key)); },
                "key"_a)
            .def(
                "set_tagged_block", [](LayerObject& self, const ffpsd::TaggedBlock& block) { self.Get().SetTaggedBlock(block); }, "block"_a)
            .def(
                "remove_tagged_block", [](LayerObject& self, std::uint32_t key) { return self.Get().RemoveTaggedBlock(key); }, "key"_a)

            .def(
                "__eq__", [](const LayerObject& x, const LayerObject& y) { return x.layer == y.layer && x.serial == y.serial; },
                nb::is_operator())
            .def("__hash__", [](const LayerObject& self) { return std::hash<const void*>()(self.layer) ^ self.serial; })
            .def("__repr__", [](const LayerObject& self) {
                const auto at = self.document->serials.find(self.layer);
                if (at == self.document->serials.end() || at->second != self.serial)
                    return std::string("<ffpsd.Layer, removed>");
                return "<ffpsd.Layer " + nb::cast<std::string>(nb::repr(nb::cast(self.layer->GetName()))) + ">";
            });
    }
} // namespace ffpsd_python
