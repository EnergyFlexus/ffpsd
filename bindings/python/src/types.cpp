#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/string.h>
#include <vector>

namespace ffpsd_python
{
    using namespace nb::literals;

    namespace
    {
        // 'lclr' when all four bytes print, 0x... otherwise.
        std::string FourccText(std::uint32_t code)
        {
            std::string text;
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                const auto c = static_cast<unsigned char>(code >> shift);
                if (!std::isprint(c))
                {
                    char hex[16];
                    std::snprintf(hex, sizeof hex, "0x%08X", static_cast<unsigned>(code));
                    return hex;
                }
                text += static_cast<char>(c);
            }
            return "'" + text + "'";
        }

        std::vector<std::uint8_t> ToVector(const BytesLike& data)
        {
            return std::vector<std::uint8_t>(data.data(), data.data() + data.size());
        }

        std::string Repr(const nb::handle& value)
        {
            return nb::cast<std::string>(nb::repr(value));
        }
    } // namespace

    void BindTypes(nb::module_& m)
    {
        nb::enum_<ffpsd::ColorMode>(m, "ColorMode")
            .value("BITMAP", ffpsd::ColorMode::kBitmap)
            .value("GRAYSCALE", ffpsd::ColorMode::kGrayscale)
            .value("INDEXED", ffpsd::ColorMode::kIndexed)
            .value("RGB", ffpsd::ColorMode::kRgb)
            .value("CMYK", ffpsd::ColorMode::kCmyk)
            .value("MULTICHANNEL", ffpsd::ColorMode::kMultichannel)
            .value("DUOTONE", ffpsd::ColorMode::kDuotone)
            .value("LAB", ffpsd::ColorMode::kLab);

        nb::enum_<ffpsd::Compression>(m, "Compression")
            .value("DEFAULT", ffpsd::Compression::kDefault)
            .value("RAW", ffpsd::Compression::kRaw)
            .value("RLE", ffpsd::Compression::kRle)
            .value("RLE_OR_RAW", ffpsd::Compression::kRleOrRaw);

        nb::enum_<ffpsd::ChannelCompression>(m, "ChannelCompression")
            .value("RAW", ffpsd::ChannelCompression::kRaw)
            .value("RLE", ffpsd::ChannelCompression::kRle)
            .value("ZIP", ffpsd::ChannelCompression::kZip)
            .value("ZIP_PREDICTION", ffpsd::ChannelCompression::kZipPrediction);

        nb::enum_<ffpsd::Anchor>(m, "Anchor")
            .value("TOP_LEFT", ffpsd::Anchor::kTopLeft)
            .value("TOP", ffpsd::Anchor::kTop)
            .value("TOP_RIGHT", ffpsd::Anchor::kTopRight)
            .value("LEFT", ffpsd::Anchor::kLeft)
            .value("CENTER", ffpsd::Anchor::kCenter)
            .value("RIGHT", ffpsd::Anchor::kRight)
            .value("BOTTOM_LEFT", ffpsd::Anchor::kBottomLeft)
            .value("BOTTOM", ffpsd::Anchor::kBottom)
            .value("BOTTOM_RIGHT", ffpsd::Anchor::kBottomRight);

        nb::enum_<ffpsd::Rotation>(m, "Rotation")
            .value("ROTATE_90", ffpsd::Rotation::k90)
            .value("ROTATE_180", ffpsd::Rotation::k180)
            .value("ROTATE_270", ffpsd::Rotation::k270);

        nb::enum_<ffpsd::FlipDirection>(m, "FlipDirection")
            .value("HORIZONTAL", ffpsd::FlipDirection::kHorizontal)
            .value("VERTICAL", ffpsd::FlipDirection::kVertical);

        nb::enum_<ffpsd::ResampleFilter>(m, "ResampleFilter")
            .value("NEAREST", ffpsd::ResampleFilter::kNearest)
            .value("BICUBIC", ffpsd::ResampleFilter::kBicubic);

        nb::enum_<ffpsd::LayerKind>(m, "LayerKind")
            .value("RASTER", ffpsd::LayerKind::kRaster)
            .value("GROUP_OPEN", ffpsd::LayerKind::kGroupOpen)
            .value("GROUP_CLOSED", ffpsd::LayerKind::kGroupClosed)
            .value("GROUP_END", ffpsd::LayerKind::kGroupEnd)
            .value("ADJUSTMENT", ffpsd::LayerKind::kAdjustment);

        nb::enum_<ffpsd::Format>(m, "Format")
            .value("PNG", ffpsd::Format::kPng)
            .value("JPEG", ffpsd::Format::kJpeg)
            .value("WEBP", ffpsd::Format::kWebp);

        nb::enum_<ffpsd::PngCompression>(m, "PngCompression")
            .value("BALANCED", ffpsd::PngCompression::kBalanced)
            .value("SMALLEST", ffpsd::PngCompression::kSmallest)
            .value("FASTEST", ffpsd::PngCompression::kFastest);

        nb::class_<ffpsd::Rect>(m, "Rect")
            .def(
                "__init__",
                [](ffpsd::Rect* self, std::int32_t top, std::int32_t left, std::int32_t bottom, std::int32_t right) {
                    new (self) ffpsd::Rect{top, left, bottom, right};
                },
                "top"_a = 0, "left"_a = 0, "bottom"_a = 0, "right"_a = 0)
            .def_rw("top", &ffpsd::Rect::top)
            .def_rw("left", &ffpsd::Rect::left)
            .def_rw("bottom", &ffpsd::Rect::bottom)
            .def_rw("right", &ffpsd::Rect::right)
            .def_prop_ro("width", &ffpsd::Rect::GetWidth)
            .def_prop_ro("height", &ffpsd::Rect::GetHeight)
            .def(
                "__eq__", [](const ffpsd::Rect& x, const ffpsd::Rect& y) { return x == y; }, nb::is_operator())
            .def("__repr__", [](const ffpsd::Rect& r) {
                return "Rect(top=" + std::to_string(r.top) + ", left=" + std::to_string(r.left) + ", bottom=" + std::to_string(r.bottom) +
                       ", right=" + std::to_string(r.right) + ")";
            });

        nb::class_<ffpsd::ImageInfo>(m, "ImageInfo")
            .def_ro("width", &ffpsd::ImageInfo::width)
            .def_ro("height", &ffpsd::ImageInfo::height)
            .def_ro("channel_count", &ffpsd::ImageInfo::channel_count)
            .def_ro("depth", &ffpsd::ImageInfo::depth)
            .def_ro("color_mode", &ffpsd::ImageInfo::color_mode)
            .def(
                "__eq__",
                [](const ffpsd::ImageInfo& x, const ffpsd::ImageInfo& y) {
                    return x.width == y.width && x.height == y.height && x.channel_count == y.channel_count && x.depth == y.depth &&
                           x.color_mode == y.color_mode;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::ImageInfo& i) {
                return "ImageInfo(width=" + std::to_string(i.width) + ", height=" + std::to_string(i.height) +
                       ", channel_count=" + std::to_string(i.channel_count) + ", depth=" + std::to_string(i.depth) +
                       ", color_mode=" + Repr(nb::cast(i.color_mode)) + ")";
            });

        nb::class_<ffpsd::ResolutionInfo>(m, "ResolutionInfo")
            .def(
                "__init__",
                [](ffpsd::ResolutionInfo* self, double horizontal, std::int16_t horizontal_unit, std::int16_t width_unit, double vertical,
                   std::int16_t vertical_unit, std::int16_t height_unit) {
                    new (self) ffpsd::ResolutionInfo{horizontal, horizontal_unit, width_unit, vertical, vertical_unit, height_unit};
                },
                "horizontal"_a = 72.0, "horizontal_unit"_a = 1, "width_unit"_a = 1, "vertical"_a = 72.0, "vertical_unit"_a = 1,
                "height_unit"_a = 1)
            .def_rw("horizontal", &ffpsd::ResolutionInfo::horizontal)
            .def_rw("horizontal_unit", &ffpsd::ResolutionInfo::horizontal_unit)
            .def_rw("width_unit", &ffpsd::ResolutionInfo::width_unit)
            .def_rw("vertical", &ffpsd::ResolutionInfo::vertical)
            .def_rw("vertical_unit", &ffpsd::ResolutionInfo::vertical_unit)
            .def_rw("height_unit", &ffpsd::ResolutionInfo::height_unit)
            .def(
                "__eq__",
                [](const ffpsd::ResolutionInfo& x, const ffpsd::ResolutionInfo& y) {
                    return x.horizontal == y.horizontal && x.horizontal_unit == y.horizontal_unit && x.width_unit == y.width_unit &&
                           x.vertical == y.vertical && x.vertical_unit == y.vertical_unit && x.height_unit == y.height_unit;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::ResolutionInfo& r) {
                return "ResolutionInfo(horizontal=" + Repr(nb::cast(r.horizontal)) + ", vertical=" + Repr(nb::cast(r.vertical)) + ")";
            });

        nb::class_<ffpsd::VersionInfo>(m, "VersionInfo")
            .def(
                "__init__",
                [](ffpsd::VersionInfo* self, std::uint32_t version, bool has_real_merged_data, std::string writer_name,
                   std::string reader_name, std::uint32_t file_version) {
                    new (self)
                        ffpsd::VersionInfo{version, has_real_merged_data, std::move(writer_name), std::move(reader_name), file_version};
                },
                "version"_a = 1, "has_real_merged_data"_a = true, "writer_name"_a = "", "reader_name"_a = "", "file_version"_a = 1)
            .def_rw("version", &ffpsd::VersionInfo::version)
            .def_rw("has_real_merged_data", &ffpsd::VersionInfo::has_real_merged_data)
            .def_rw("writer_name", &ffpsd::VersionInfo::writer_name)
            .def_rw("reader_name", &ffpsd::VersionInfo::reader_name)
            .def_rw("file_version", &ffpsd::VersionInfo::file_version)
            .def(
                "__eq__",
                [](const ffpsd::VersionInfo& x, const ffpsd::VersionInfo& y) {
                    return x.version == y.version && x.has_real_merged_data == y.has_real_merged_data && x.writer_name == y.writer_name &&
                           x.reader_name == y.reader_name && x.file_version == y.file_version;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::VersionInfo& v) {
                return "VersionInfo(writer_name=" + Repr(nb::cast(v.writer_name)) + ", reader_name=" + Repr(nb::cast(v.reader_name)) +
                       ", has_real_merged_data=" + (v.has_real_merged_data ? "True" : "False") + ")";
            });

        nb::class_<ffpsd::ImageResource>(m, "ImageResource")
            .def(
                "__init__",
                [](ffpsd::ImageResource* self, std::uint16_t id, std::string name, const BytesLike& data) {
                    new (self) ffpsd::ImageResource{id, std::move(name), ToVector(data)};
                },
                "id"_a, "name"_a = "", "data"_a = nb::bytes("", 0))
            .def_rw("id", &ffpsd::ImageResource::id)
            .def_rw("name", &ffpsd::ImageResource::name)
            .def_prop_rw(
                "data", [](const ffpsd::ImageResource& r) { return ToBytes(r.data.data(), r.data.size()); },
                [](ffpsd::ImageResource& r, const BytesLike& data) { r.data = ToVector(data); })
            .def(
                "__eq__",
                [](const ffpsd::ImageResource& x, const ffpsd::ImageResource& y) {
                    return x.id == y.id && x.name == y.name && x.data == y.data;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::ImageResource& r) {
                return "ImageResource(id=" + std::to_string(r.id) + ", name=" + Repr(nb::cast(r.name)) + ", data=<" +
                       std::to_string(r.data.size()) + " bytes>)";
            });

        nb::class_<ffpsd::TaggedBlock>(m, "TaggedBlock")
            .def(
                "__init__",
                [](ffpsd::TaggedBlock* self, std::uint32_t key, const BytesLike& data, std::uint32_t signature) {
                    new (self) ffpsd::TaggedBlock{signature, key, ToVector(data)};
                },
                "key"_a, "data"_a = nb::bytes("", 0), "signature"_a = ffpsd::TaggedBlock().signature)
            .def_rw("signature", &ffpsd::TaggedBlock::signature)
            .def_rw("key", &ffpsd::TaggedBlock::key)
            .def_prop_rw(
                "data", [](const ffpsd::TaggedBlock& b) { return ToBytes(b.data.data(), b.data.size()); },
                [](ffpsd::TaggedBlock& b, const BytesLike& data) { b.data = ToVector(data); })
            .def(
                "__eq__",
                [](const ffpsd::TaggedBlock& x, const ffpsd::TaggedBlock& y) {
                    return x.signature == y.signature && x.key == y.key && x.data == y.data;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::TaggedBlock& b) {
                return "TaggedBlock(key=" + FourccText(b.key) + ", data=<" + std::to_string(b.data.size()) +
                       " bytes>, signature=" + FourccText(b.signature) + ")";
            });

        using Channel = ffpsd::LevelsInfo::Channel;
        const auto same = [](const Channel& x, const Channel& y) {
            return x.input_floor == y.input_floor && x.input_ceiling == y.input_ceiling && x.output_floor == y.output_floor &&
                   x.output_ceiling == y.output_ceiling && x.gamma == y.gamma;
        };
        nb::class_<Channel>(m, "LevelsChannel")
            .def(
                "__init__",
                [](Channel* self, std::uint16_t input_floor, std::uint16_t input_ceiling, std::uint16_t output_floor,
                   std::uint16_t output_ceiling,
                   double gamma) { new (self) Channel{input_floor, input_ceiling, output_floor, output_ceiling, gamma}; },
                "input_floor"_a = 0, "input_ceiling"_a = 255, "output_floor"_a = 0, "output_ceiling"_a = 255, "gamma"_a = 1.0)
            .def_rw("input_floor", &Channel::input_floor)
            .def_rw("input_ceiling", &Channel::input_ceiling)
            .def_rw("output_floor", &Channel::output_floor)
            .def_rw("output_ceiling", &Channel::output_ceiling)
            .def_rw("gamma", &Channel::gamma)
            .def("__eq__", same, nb::is_operator())
            .def("__repr__", [](const Channel& c) {
                return "LevelsChannel(input_floor=" + std::to_string(c.input_floor) + ", input_ceiling=" + std::to_string(c.input_ceiling) +
                       ", output_floor=" + std::to_string(c.output_floor) + ", output_ceiling=" + std::to_string(c.output_ceiling) +
                       ", gamma=" + Repr(nb::cast(c.gamma)) + ")";
            });

        // A list that changes the Levels it came from, so levels.channels[1].gamma = 2 sticks.
        nb::bind_vector<std::vector<Channel>>(m, "LevelsChannelList");

        nb::class_<ffpsd::LevelsInfo>(m, "Levels")
            .def(
                "__init__",
                [](ffpsd::LevelsInfo* self, std::vector<Channel> channels) { new (self) ffpsd::LevelsInfo{std::move(channels)}; },
                "channels"_a = std::vector<Channel>(),
                // Any iterable converts, a list too, which the stub would otherwise not let through.
                nb::sig("def __init__(self, channels: collections.abc.Iterable[LevelsChannel] = ...) -> None"))
            .def_rw("channels", &ffpsd::LevelsInfo::channels)
            .def(
                "__eq__",
                [same](const ffpsd::LevelsInfo& x, const ffpsd::LevelsInfo& y) {
                    return std::equal(x.channels.begin(), x.channels.end(), y.channels.begin(), y.channels.end(), same);
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::LevelsInfo& l) {
                std::string text = "Levels(channels=[";
                for (std::size_t i = 0; i < l.channels.size(); ++i)
                    text += (i ? ", " : "") + Repr(nb::cast(l.channels[i]));
                return text + "])";
            });

        nb::class_<ffpsd::ChannelInfo>(m, "ChannelInfo")
            .def_ro("id", &ffpsd::ChannelInfo::id)
            .def_ro("compression", &ffpsd::ChannelInfo::compression)
            .def_ro("size", &ffpsd::ChannelInfo::size)
            .def(
                "__eq__",
                [](const ffpsd::ChannelInfo& x, const ffpsd::ChannelInfo& y) {
                    return x.id == y.id && x.compression == y.compression && x.size == y.size;
                },
                nb::is_operator())
            .def("__repr__", [](const ffpsd::ChannelInfo& c) {
                return "ChannelInfo(id=" + std::to_string(c.id) + ", compression=" + Repr(nb::cast(c.compression)) +
                       ", size=" + std::to_string(c.size) + ")";
            });
    }
} // namespace ffpsd_python
