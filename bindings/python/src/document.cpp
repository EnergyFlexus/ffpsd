#include "common.hpp"

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <vector>

namespace ffpsd_python
{
    using namespace nb::literals;

    namespace
    {
        DocumentObject Opened(ffpsd::Document&& document)
        {
            return DocumentObject(std::move(document));
        }

        template <class T> std::optional<T> Copy(const T* found)
        {
            return found ? std::optional<T>(*found) : std::nullopt;
        }
    } // namespace

    void BindDocument(nb::module_& m)
    {
        nb::class_<DocumentObject>(m, "Document")
            .def(
                "__init__",
                [](DocumentObject* self, std::uint32_t width, std::uint32_t height, ffpsd::ColorMode color_mode, std::uint16_t depth) {
                    new (self) DocumentObject(ffpsd::Document(width, height, color_mode, depth));
                },
                "width"_a, "height"_a, "color_mode"_a = ffpsd::ColorMode::kRgb, "depth"_a = 8)
            .def_static(
                "open",
                [](const std::filesystem::path& path) {
                    const std::string utf8 = ToUtf8(path);
                    nb::gil_scoped_release release;
                    return Opened(ffpsd::Document::Open(utf8));
                },
                "path"_a)
            .def_static(
                "from_bytes",
                [](const BytesLike& data) {
                    nb::gil_scoped_release release;
                    return Opened(ffpsd::Document::Parse(data.data(), data.size()));
                },
                "data"_a)
            .def(
                "save",
                [](const DocumentObject& self, const std::filesystem::path& path, ffpsd::Compression compression) {
                    const std::string utf8 = ToUtf8(path);
                    WithoutGil(self, [&] { self.document.Save(utf8, compression); });
                },
                "path"_a, "compression"_a = ffpsd::Compression::kDefault)
            .def(
                "to_bytes",
                [](const DocumentObject& self, ffpsd::Compression compression) {
                    const std::vector<std::uint8_t> data = WithoutGil(self, [&] { return self.document.Save(compression); });
                    return ToBytes(data.data(), data.size());
                },
                "compression"_a = ffpsd::Compression::kDefault)

            .def_prop_ro("width", [](const DocumentObject& self) { return self.Get().GetWidth(); })
            .def_prop_ro("height", [](const DocumentObject& self) { return self.Get().GetHeight(); })
            .def_prop_ro("channel_count", [](const DocumentObject& self) { return self.Get().GetChannelCount(); })
            .def_prop_ro("depth", [](const DocumentObject& self) { return self.Get().GetDepth(); })
            .def_prop_ro("color_mode", [](const DocumentObject& self) { return self.Get().GetColorMode(); })
            .def_prop_rw(
                "psb", [](const DocumentObject& self) { return self.Get().IsPsb(); },
                [](DocumentObject& self, bool psb) { self.Get().SetPsb(psb); })
            .def_prop_rw(
                "has_real_merged_data", [](const DocumentObject& self) { return self.Get().HasRealMergedData(); },
                [](DocumentObject& self, bool value) { self.Get().SetHasRealMergedData(value); })
            .def_prop_rw(
                "version_info", [](const DocumentObject& self) { return self.Get().GetVersionInfo(); },
                [](DocumentObject& self, const ffpsd::VersionInfo& value) { self.Get().SetVersionInfo(value); })
            .def_prop_rw(
                "resolution_info", [](const DocumentObject& self) { return self.Get().GetResolutionInfo(); },
                [](DocumentObject& self, const ffpsd::ResolutionInfo& value) { self.Get().SetResolutionInfo(value); })

            .def(
                "resize_canvas",
                [](DocumentObject& self, std::uint32_t width, std::uint32_t height, ffpsd::Anchor anchor) {
                    WithoutGil(self, [&] { self.document.ResizeCanvas(width, height, anchor); });
                },
                "width"_a, "height"_a, "anchor"_a = ffpsd::Anchor::kCenter)
            .def(
                "resize",
                [](DocumentObject& self, std::uint32_t width, std::uint32_t height, ffpsd::ResampleFilter filter) {
                    WithoutGil(self, [&] { self.document.Resize(width, height, filter); });
                },
                "width"_a, "height"_a, "filter"_a = ffpsd::ResampleFilter::kBicubic)
            .def(
                "flip_canvas",
                [](DocumentObject& self, ffpsd::FlipDirection direction) {
                    WithoutGil(self, [&] { self.document.FlipCanvas(direction); });
                },
                "direction"_a)
            .def(
                "rotate_canvas",
                [](DocumentObject& self, ffpsd::Rotation rotation) { WithoutGil(self, [&] { self.document.RotateCanvas(rotation); }); },
                "rotation"_a)
            .def(
                "convert_color_mode",
                [](DocumentObject& self, ffpsd::ColorMode color_mode) {
                    WithoutGil(self, [&] { self.document.ConvertColorMode(color_mode); });
                },
                "color_mode"_a)

            .def_prop_ro(
                "image_resources",
                [](const DocumentObject& self) {
                    std::vector<ffpsd::ImageResource> resources;
                    for (std::size_t i = 0; i < self.Get().GetImageResourceCount(); ++i)
                        resources.push_back(*self.Get().GetImageResourceByIndex(i));
                    return resources;
                })
            .def(
                "find_image_resource",
                [](const DocumentObject& self, std::uint16_t id) { return Copy(self.Get().GetImageResourceById(id)); }, "id"_a)
            .def(
                "set_image_resource",
                [](DocumentObject& self, const ffpsd::ImageResource& resource) { self.Get().SetImageResource(resource); }, "resource"_a)
            .def(
                "remove_image_resource", [](DocumentObject& self, std::uint16_t id) { return self.Get().RemoveImageResource(id); }, "id"_a)

            .def_prop_ro("layer_count", [](const DocumentObject& self) { return self.Get().GetLayerCount(); })
            .def_prop_ro(
                "layers",
                [](DocumentObject& self) {
                    std::vector<LayerObject> layers;
                    for (std::size_t i = 0; i < self.Get().GetLayerCount(); ++i)
                        layers.push_back(MakeLayer(self, self.Get().GetLayerByIndex(i)));
                    return layers;
                })
            .def(
                "layer",
                [](DocumentObject& self, std::int64_t index) { return MakeLayer(self, self.Get().GetLayerByIndex(self.Index(index))); },
                "index"_a)
            .def(
                "find_layer_index",
                [](const DocumentObject& self, const LayerObject& layer) { return self.Get().FindLayerIndex(layer.Get()); }, "layer"_a)
            .def(
                "add_layer",
                [](DocumentObject& self, const std::string& name, const std::optional<PixelArray>& pixels, std::int32_t top,
                   std::int32_t left) {
                    const ffpsd::ImageView view = ToView(pixels, self.Get().GetColorMode());
                    return MakeLayer(self, WithoutGil(self, [&] { return self.document.AddLayer(name, view, top, left); }));
                },
                "name"_a, "pixels"_a = nb::none(), "top"_a = 0, "left"_a = 0)
            .def(
                "add_background_layer",
                [](DocumentObject& self, const std::string& name, const std::optional<PixelArray>& pixels) {
                    const ffpsd::ImageView view = ToView(pixels, self.Get().GetColorMode());
                    return MakeLayer(self, WithoutGil(self, [&] { return self.document.AddBackgroundLayer(name, view); }));
                },
                "name"_a, "pixels"_a = nb::none())
            .def(
                "set_background_layer", [](DocumentObject& self, std::int64_t index) { self.Get().SetBackgroundLayer(self.Index(index)); },
                "index"_a)
            .def("unset_background_layer", [](DocumentObject& self) { return self.Get().UnsetBackgroundLayer(); })
            .def(
                "add_group", [](DocumentObject& self, const std::string& name) { return MakeLayer(self, self.Get().AddGroup(name)); },
                "name"_a)
            .def(
                "add_levels_layer",
                [](DocumentObject& self, const std::string& name, const std::optional<ffpsd::LevelsInfo>& levels) {
                    return MakeLayer(self, self.Get().AddAdjustmentLayer(name, levels.value_or(ffpsd::LevelsInfo())));
                },
                "name"_a, "levels"_a = nb::none())
            .def(
                "add_layer_copy",
                [](DocumentObject& self, const LayerObject& source) { return MakeLayer(self, self.Get().AddLayerCopy(source.Get())); },
                "source"_a)
            .def(
                "duplicate_layer",
                [](DocumentObject& self, std::int64_t index) {
                    return MakeLayer(self, self.Get().AddLayerCopy(*self.Get().GetLayerByIndex(self.Index(index))));
                },
                "index"_a)
            .def(
                "remove_layer",
                [](DocumentObject& self, std::int64_t index) {
                    self.Get().RemoveLayer(self.Index(index));
                    self.ForgetRemoved();
                },
                "index"_a)
            .def(
                "move_layer",
                [](DocumentObject& self, std::int64_t index, std::int64_t to) { self.Get().MoveLayer(self.Index(index), self.Index(to)); },
                "index"_a, "to"_a)

            .def_prop_ro(
                "tagged_blocks",
                [](const DocumentObject& self) {
                    std::vector<ffpsd::TaggedBlock> blocks;
                    for (std::size_t i = 0; i < self.Get().GetTaggedBlockCount(); ++i)
                        blocks.push_back(*self.Get().GetTaggedBlockByIndex(i));
                    return blocks;
                })
            .def(
                "find_tagged_block",
                [](const DocumentObject& self, std::uint32_t key) { return Copy(self.Get().GetTaggedBlockByKey(key)); }, "key"_a)
            .def(
                "set_tagged_block", [](DocumentObject& self, const ffpsd::TaggedBlock& block) { self.Get().SetTaggedBlock(block); },
                "block"_a)
            .def(
                "remove_tagged_block", [](DocumentObject& self, std::uint32_t key) { return self.Get().RemoveTaggedBlock(key); }, "key"_a)

            .def(
                "merged_image",
                [](const DocumentObject& self) -> std::optional<NumpyArray> {
                    ffpsd::Image image = WithoutGil(self, [&] { return self.document.GetMergedImage(); });
                    if (image.IsEmpty())
                        return std::nullopt;
                    return ToArray(std::move(image));
                })
            .def(
                "merged_image_info",
                [](const DocumentObject& self) -> std::optional<ffpsd::ImageInfo> {
                    const ffpsd::ImageInfo info = self.Get().GetMergedImageInfo();
                    return info.IsEmpty() ? std::nullopt : std::optional<ffpsd::ImageInfo>(info);
                })
            .def(
                "set_merged_image",
                [](DocumentObject& self, const PixelArray& pixels) {
                    const ffpsd::ImageView view = ToView(pixels, self.Get().GetColorMode());
                    WithoutGil(self, [&] { self.document.SetMergedImage(view); });
                },
                "pixels"_a)
            .def_prop_ro("merged_compression", [](const DocumentObject& self) { return self.Get().GetMergedCompression(); })

            .def("__repr__", [](const DocumentObject& self) {
                if (self.busy)
                    return std::string("<ffpsd.Document, used by another thread>");
                const ffpsd::Document& d = self.document;
                return "<ffpsd.Document " + std::to_string(d.GetWidth()) + " x " + std::to_string(d.GetHeight()) + ", " +
                       std::to_string(d.GetDepth()) + " bit, " + std::to_string(d.GetLayerCount()) + " layers>";
            });
    }
} // namespace ffpsd_python
