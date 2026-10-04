#include "detail/formats/picture.hpp"
#include "detail/io/file.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <ffpsd/c_api.h>
#include <ffpsd/document.hpp>
#include <ffpsd/formats.hpp>
#include <ffpsd/layer.hpp>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

struct ffpsd_document_t
{
    ffpsd::Document value;
};
struct ffpsd_image_t
{
    ffpsd::Image value;
};
struct ffpsd_buffer_t
{
    std::vector<std::uint8_t> value;
};
struct ffpsd_version_info_t
{
    ffpsd::VersionInfo value;
};
struct ffpsd_levels_t
{
    ffpsd::LevelsInfo value;
};

namespace
{
    static_assert(static_cast<int>(ffpsd::LayerKind::kAdjustment) == FFPSD_LAYER_KIND_ADJUSTMENT);
    static_assert(static_cast<int>(ffpsd::ColorMode::kLab) == FFPSD_COLOR_MODE_LAB);
    static_assert(static_cast<int>(ffpsd::ResampleFilter::kBicubic) == FFPSD_RESAMPLE_FILTER_BICUBIC);
    static_assert(static_cast<int>(ffpsd::Anchor::kBottomRight) == FFPSD_ANCHOR_BOTTOM_RIGHT);
    static_assert(static_cast<int>(ffpsd::Rotation::k270) == FFPSD_ROTATION_270);
    static_assert(static_cast<int>(ffpsd::FlipDirection::kVertical) == FFPSD_FLIP_VERTICAL);
    static_assert(static_cast<int>(ffpsd::Compression::kRle) == FFPSD_COMPRESSION_RLE);
    static_assert(static_cast<int>(ffpsd::Compression::kRleOrRaw) == FFPSD_COMPRESSION_RLE_OR_RAW);

    thread_local std::string g_last_error;

    // Thrown inside a call and turned into their own statuses.
    struct NotFound : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    struct Unsupported : std::logic_error
    {
        using std::logic_error::logic_error;
    };

    ffpsd_status_t Fail(ffpsd_status_t status, const char* message) noexcept
    {
        try
        {
            g_last_error = message;
        }
        catch (...)
        {
            g_last_error.clear();
        }
        return status;
    }

    // Derived exceptions are caught before their bases: invalid_argument is a logic_error.
    template <typename Body> ffpsd_status_t Guard(Body&& body) noexcept
    {
        g_last_error.clear();
        try
        {
            body();
            return FFPSD_STATUS_OK;
        }
        catch (const NotFound& e)
        {
            return Fail(FFPSD_STATUS_NOT_FOUND, e.what());
        }
        catch (const Unsupported& e)
        {
            return Fail(FFPSD_STATUS_UNSUPPORTED, e.what());
        }
        catch (const std::invalid_argument& e)
        {
            return Fail(FFPSD_STATUS_INVALID_ARGUMENT, e.what());
        }
        catch (const std::out_of_range& e)
        {
            return Fail(FFPSD_STATUS_OUT_OF_RANGE, e.what());
        }
        catch (const std::length_error& e)
        {
            return Fail(FFPSD_STATUS_INVALID_ARGUMENT, e.what());
        }
        catch (const std::logic_error& e)
        {
            return Fail(FFPSD_STATUS_INVALID_OPERATION, e.what());
        }
        catch (const std::system_error& e)
        {
            return Fail(FFPSD_STATUS_IO, e.what());
        }
        catch (const std::runtime_error& e)
        {
            return Fail(FFPSD_STATUS_INVALID_FILE, e.what());
        }
        catch (const std::bad_alloc&)
        {
            return Fail(FFPSD_STATUS_OUT_OF_MEMORY, "ffpsd: out of memory");
        }
        catch (const std::exception& e)
        {
            return Fail(FFPSD_STATUS_UNKNOWN, e.what());
        }
        catch (...)
        {
            return Fail(FFPSD_STATUS_UNKNOWN, "ffpsd: unknown error");
        }
    }

    template <typename T> T& Need(T* pointer, const char* what)
    {
        if (pointer == nullptr)
            throw std::invalid_argument(std::string("ffpsd: ") + what + " is NULL");
        return *pointer;
    }

    // Checked and cleared first, so a failed call never leaves a stale handle behind.
    template <typename T> T*& NeedOut(T** out)
    {
        T*& target = Need(out, "out");
        target = nullptr;
        return target;
    }

    ffpsd::Layer& ToLayer(ffpsd_layer_t* layer)
    {
        return *reinterpret_cast<ffpsd::Layer*>(&Need(layer, "layer"));
    }
    const ffpsd::Layer& ToLayer(const ffpsd_layer_t* layer)
    {
        return *reinterpret_cast<const ffpsd::Layer*>(&Need(layer, "layer"));
    }
    ffpsd_layer_t* ToHandle(ffpsd::Layer* layer) noexcept
    {
        return reinterpret_cast<ffpsd_layer_t*>(layer);
    }

    std::string ToString(const char* text)
    {
        return text == nullptr ? std::string() : std::string(text);
    }

    const std::uint8_t* NeedBytes(const std::uint8_t* data, std::size_t size)
    {
        if (data == nullptr && size != 0)
            throw std::invalid_argument("ffpsd: data is NULL with a size of " + std::to_string(size));
        return data;
    }

    ffpsd::ImageView ToView(const ffpsd_image_view_t& view)
    {
        ffpsd::ImageView image;
        image.width = view.width;
        image.height = view.height;
        image.channel_count = view.channel_count;
        image.depth = view.depth;
        image.color_mode = static_cast<ffpsd::ColorMode>(view.color_mode);
        image.data = NeedBytes(view.data, view.size);
        image.size = view.size;
        return image;
    }

    ffpsd_image_t* NewImage(ffpsd::Image image)
    {
        return new ffpsd_image_t{std::move(image)};
    }

    void Fill(const ffpsd::ImageResource& resource, ffpsd_image_resource_t& out) noexcept
    {
        out.id = resource.id;
        out.name = resource.name.c_str();
        out.data = resource.data.data();
        out.size = resource.data.size();
    }

    void Fill(const ffpsd::Rect& rect, ffpsd_rect_t& out) noexcept
    {
        out.top = rect.top;
        out.left = rect.left;
        out.bottom = rect.bottom;
        out.right = rect.right;
    }

    void Fill(const ffpsd::TaggedBlock& block, ffpsd_tagged_block_t& out) noexcept
    {
        out.signature = block.signature;
        out.key = block.key;
        out.data = block.data.data();
        out.size = block.data.size();
    }

    ffpsd::TaggedBlock ToBlock(const ffpsd_tagged_block_t& block)
    {
        const std::uint8_t* data = NeedBytes(block.data, block.size);

        ffpsd::TaggedBlock result;
        result.signature = block.signature;
        result.key = block.key;
        if (block.size != 0)
            result.data.assign(data, data + block.size);
        return result;
    }

    ffpsd::ImageResource ToResource(const ffpsd_image_resource_t& resource)
    {
        const std::uint8_t* data = NeedBytes(resource.data, resource.size);

        ffpsd::ImageResource result;
        result.id = resource.id;
        result.name = ToString(resource.name);
        if (resource.size != 0)
            result.data.assign(data, data + resource.size);
        return result;
    }

    void RequirePng()
    {
#if !defined(FFPSD_HAS_PNG)
        throw Unsupported("ffpsd: built without PNG support");
#endif
    }

#if defined(FFPSD_HAS_PNG)
    using ffpsd::EncodePng;
    using ffpsd::LoadPng;
    using ffpsd::SavePng;
#else
    // Never reached: RequirePng throws first. They keep the PNG entry points below compiling.
    ffpsd::Image LoadPng(const std::string&, ffpsd::ColorMode, std::uint16_t)
    {
        return ffpsd::Image();
    }
    ffpsd::Image LoadPng(const std::uint8_t*, std::size_t, ffpsd::ColorMode, std::uint16_t)
    {
        return ffpsd::Image();
    }
    std::vector<std::uint8_t> EncodePng(const ffpsd::ImageView&)
    {
        return {};
    }
    void SavePng(const ffpsd::ImageView&, const std::string&)
    {
    }
#endif

    void RequireJpeg()
    {
#if !defined(FFPSD_HAS_JPEG)
        throw Unsupported("ffpsd: built without JPEG support");
#endif
    }

#if defined(FFPSD_HAS_JPEG)
    using ffpsd::EncodeJpeg;
    using ffpsd::LoadJpeg;
    using ffpsd::SaveJpeg;
#else
    // Never reached: RequireJpeg throws first. They keep the JPEG entry points below compiling.
    ffpsd::Image LoadJpeg(const std::string&, ffpsd::ColorMode, std::uint16_t, bool)
    {
        return ffpsd::Image();
    }
    ffpsd::Image LoadJpeg(const std::uint8_t*, std::size_t, ffpsd::ColorMode, std::uint16_t, bool)
    {
        return ffpsd::Image();
    }
    std::vector<std::uint8_t> EncodeJpeg(const ffpsd::ImageView&, int)
    {
        return {};
    }
    void SaveJpeg(const ffpsd::ImageView&, const std::string&, int)
    {
    }
#endif

    // UNSUPPORTED for a format this build lacks, as every PNG and JPEG call reports it.
    ffpsd::Image LoadPicture(const std::uint8_t* data, std::size_t size, ffpsd_color_mode_t color_mode, std::uint16_t depth)
    {
        const std::optional<ffpsd::Format> format = ffpsd::detail::FindFormat(data, size);
        if (format == ffpsd::Format::kPng)
            RequirePng();
        if (format == ffpsd::Format::kJpeg)
            RequireJpeg();
        return ffpsd::LoadPicture(data, size, static_cast<ffpsd::ColorMode>(color_mode), depth);
    }

    template <typename T> const T* FoundOrThrow(const T* found, const char* what, std::uint32_t id)
    {
        if (found == nullptr)
            throw NotFound(std::string("ffpsd: no ") + what + " " + std::to_string(id));
        return found;
    }
} // namespace

extern "C"
{
    const char* ffpsd_version(void)
    {
        return FFPSD_VERSION;
    }

    const char* ffpsd_last_error(void)
    {
        return g_last_error.c_str();
    }

    const uint8_t* ffpsd_buffer_get_data(const ffpsd_buffer_t* buffer)
    {
        return buffer == nullptr ? nullptr : buffer->value.data();
    }
    size_t ffpsd_buffer_get_size(const ffpsd_buffer_t* buffer)
    {
        return buffer == nullptr ? 0 : buffer->value.size();
    }
    void ffpsd_buffer_destroy(ffpsd_buffer_t* buffer)
    {
        delete buffer;
    }

    ffpsd_status_t ffpsd_image_get_view(const ffpsd_image_t* image, ffpsd_image_view_t* out)
    {
        return Guard([&] {
            const ffpsd::Image& value = Need(image, "image").value;
            ffpsd_image_view_t& view = Need(out, "out");
            view.width = value.width;
            view.height = value.height;
            view.channel_count = value.channel_count;
            view.depth = value.depth;
            view.color_mode = static_cast<ffpsd_color_mode_t>(value.color_mode);
            view.data = value.bytes.data();
            view.size = value.bytes.size();
        });
    }
    void ffpsd_image_destroy(ffpsd_image_t* image)
    {
        delete image;
    }

    ffpsd_status_t ffpsd_version_info_create(ffpsd_version_info_t** out)
    {
        return Guard([&] { NeedOut(out) = new ffpsd_version_info_t(); });
    }
    void ffpsd_version_info_destroy(ffpsd_version_info_t* info)
    {
        delete info;
    }
    uint32_t ffpsd_version_info_get_version(const ffpsd_version_info_t* info)
    {
        return info == nullptr ? 0 : info->value.version;
    }
    int ffpsd_version_info_get_has_real_merged_data(const ffpsd_version_info_t* info)
    {
        return info != nullptr && info->value.has_real_merged_data ? 1 : 0;
    }
    const char* ffpsd_version_info_get_writer_name(const ffpsd_version_info_t* info)
    {
        return info == nullptr ? nullptr : info->value.writer_name.c_str();
    }
    const char* ffpsd_version_info_get_reader_name(const ffpsd_version_info_t* info)
    {
        return info == nullptr ? nullptr : info->value.reader_name.c_str();
    }
    uint32_t ffpsd_version_info_get_file_version(const ffpsd_version_info_t* info)
    {
        return info == nullptr ? 0 : info->value.file_version;
    }
    ffpsd_status_t ffpsd_version_info_set_version(ffpsd_version_info_t* info, uint32_t version)
    {
        return Guard([&] { Need(info, "info").value.version = version; });
    }
    ffpsd_status_t ffpsd_version_info_set_has_real_merged_data(ffpsd_version_info_t* info, int value)
    {
        return Guard([&] { Need(info, "info").value.has_real_merged_data = value != 0; });
    }
    ffpsd_status_t ffpsd_version_info_set_writer_name(ffpsd_version_info_t* info, const char* name)
    {
        return Guard([&] { Need(info, "info").value.writer_name = ToString(name); });
    }
    ffpsd_status_t ffpsd_version_info_set_reader_name(ffpsd_version_info_t* info, const char* name)
    {
        return Guard([&] { Need(info, "info").value.reader_name = ToString(name); });
    }
    ffpsd_status_t ffpsd_version_info_set_file_version(ffpsd_version_info_t* info, uint32_t version)
    {
        return Guard([&] { Need(info, "info").value.file_version = version; });
    }

    ffpsd_status_t ffpsd_levels_create(ffpsd_levels_t** out)
    {
        return Guard([&] { NeedOut(out) = new ffpsd_levels_t(); });
    }
    void ffpsd_levels_destroy(ffpsd_levels_t* levels)
    {
        delete levels;
    }
    size_t ffpsd_levels_get_count(const ffpsd_levels_t* levels)
    {
        return levels == nullptr ? 0 : levels->value.channels.size();
    }
    ffpsd_status_t ffpsd_levels_get_channel(const ffpsd_levels_t* levels, size_t index, ffpsd_levels_channel_t* out)
    {
        return Guard([&] {
            const std::vector<ffpsd::LevelsInfo::Channel>& channels = Need(levels, "levels").value.channels;
            ffpsd_levels_channel_t& channel = Need(out, "out");
            if (index >= channels.size())
                throw std::out_of_range("ffpsd: Levels record " + std::to_string(index) + " of " + std::to_string(channels.size()));

            channel.input_floor = channels[index].input_floor;
            channel.input_ceiling = channels[index].input_ceiling;
            channel.output_floor = channels[index].output_floor;
            channel.output_ceiling = channels[index].output_ceiling;
            channel.gamma = channels[index].gamma;
        });
    }
    ffpsd_status_t ffpsd_levels_set_channel(ffpsd_levels_t* levels, size_t index, const ffpsd_levels_channel_t* channel)
    {
        return Guard([&] {
            std::vector<ffpsd::LevelsInfo::Channel>& channels = Need(levels, "levels").value.channels;
            const ffpsd_levels_channel_t& from = Need(channel, "channel");
            if (index >= channels.size())
                channels.resize(index + 1);

            channels[index].input_floor = from.input_floor;
            channels[index].input_ceiling = from.input_ceiling;
            channels[index].output_floor = from.output_floor;
            channels[index].output_ceiling = from.output_ceiling;
            channels[index].gamma = from.gamma;
        });
    }

    ffpsd_status_t
    ffpsd_document_create_with(uint32_t width, uint32_t height, ffpsd_color_mode_t color_mode, uint16_t depth, ffpsd_document_t** out)
    {
        return Guard([&] {
            ffpsd_document_t*& target = NeedOut(out);
            target = new ffpsd_document_t{ffpsd::Document(width, height, static_cast<ffpsd::ColorMode>(color_mode), depth)};
        });
    }
    ffpsd_status_t ffpsd_document_open(const char* path, ffpsd_document_t** out)
    {
        return Guard([&] {
            ffpsd_document_t*& target = NeedOut(out);
            target = new ffpsd_document_t{ffpsd::Document::Open(std::string(&Need(path, "path")))};
        });
    }
    ffpsd_status_t ffpsd_document_open_memory(const uint8_t* data, size_t size, ffpsd_document_t** out)
    {
        return Guard([&] {
            ffpsd_document_t*& target = NeedOut(out);
            target = new ffpsd_document_t{ffpsd::Document::Parse(NeedBytes(data, size), size)};
        });
    }
    ffpsd_status_t ffpsd_document_save(const ffpsd_document_t* doc, const char* path, ffpsd_compression_t compression)
    {
        return Guard([&] { Need(doc, "doc").value.Save(std::string(&Need(path, "path")), static_cast<ffpsd::Compression>(compression)); });
    }
    ffpsd_status_t ffpsd_document_save_memory(const ffpsd_document_t* doc, ffpsd_compression_t compression, ffpsd_buffer_t** out)
    {
        return Guard([&] {
            ffpsd_buffer_t*& target = NeedOut(out);
            target = new ffpsd_buffer_t{Need(doc, "doc").value.Save(static_cast<ffpsd::Compression>(compression))};
        });
    }
    void ffpsd_document_destroy(ffpsd_document_t* doc)
    {
        delete doc;
    }

    uint32_t ffpsd_document_get_width(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetWidth();
    }
    uint32_t ffpsd_document_get_height(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetHeight();
    }
    uint16_t ffpsd_document_get_channel_count(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetChannelCount();
    }
    uint16_t ffpsd_document_get_depth(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetDepth();
    }
    ffpsd_color_mode_t ffpsd_document_get_color_mode(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? FFPSD_COLOR_MODE_BITMAP : static_cast<ffpsd_color_mode_t>(doc->value.GetColorMode());
    }
    int ffpsd_document_is_psb(const ffpsd_document_t* doc)
    {
        return doc != nullptr && doc->value.IsPsb() ? 1 : 0;
    }
    int ffpsd_document_get_has_real_merged_data(const ffpsd_document_t* doc)
    {
        return doc != nullptr && doc->value.HasRealMergedData() ? 1 : 0;
    }

    ffpsd_status_t ffpsd_document_resize_canvas(ffpsd_document_t* doc, uint32_t width, uint32_t height, ffpsd_anchor_t anchor)
    {
        return Guard([&] { Need(doc, "doc").value.ResizeCanvas(width, height, static_cast<ffpsd::Anchor>(anchor)); });
    }
    ffpsd_status_t ffpsd_document_resize(ffpsd_document_t* doc, uint32_t width, uint32_t height, ffpsd_resample_filter_t filter)
    {
        return Guard([&] { Need(doc, "doc").value.Resize(width, height, static_cast<ffpsd::ResampleFilter>(filter)); });
    }
    ffpsd_status_t ffpsd_document_flip_canvas(ffpsd_document_t* doc, ffpsd_flip_direction_t direction)
    {
        return Guard([&] { Need(doc, "doc").value.FlipCanvas(static_cast<ffpsd::FlipDirection>(direction)); });
    }
    ffpsd_status_t ffpsd_document_rotate_canvas(ffpsd_document_t* doc, ffpsd_rotation_t rotation)
    {
        return Guard([&] { Need(doc, "doc").value.RotateCanvas(static_cast<ffpsd::Rotation>(rotation)); });
    }
    ffpsd_status_t ffpsd_document_convert_color_mode(ffpsd_document_t* doc, ffpsd_color_mode_t color_mode)
    {
        return Guard([&] { Need(doc, "doc").value.ConvertColorMode(static_cast<ffpsd::ColorMode>(color_mode)); });
    }
    ffpsd_status_t ffpsd_document_set_psb(ffpsd_document_t* doc, int psb)
    {
        return Guard([&] { Need(doc, "doc").value.SetPsb(psb != 0); });
    }
    ffpsd_status_t ffpsd_document_set_has_real_merged_data(ffpsd_document_t* doc, int value)
    {
        return Guard([&] { Need(doc, "doc").value.SetHasRealMergedData(value != 0); });
    }

    ffpsd_status_t ffpsd_document_get_resolution_info(const ffpsd_document_t* doc, ffpsd_resolution_info_t* out)
    {
        return Guard([&] {
            const ffpsd::ResolutionInfo info = Need(doc, "doc").value.GetResolutionInfo();
            ffpsd_resolution_info_t& target = Need(out, "out");
            target.horizontal = info.horizontal;
            target.horizontal_unit = info.horizontal_unit;
            target.width_unit = info.width_unit;
            target.vertical = info.vertical;
            target.vertical_unit = info.vertical_unit;
            target.height_unit = info.height_unit;
        });
    }
    ffpsd_status_t ffpsd_document_set_resolution_info(ffpsd_document_t* doc, const ffpsd_resolution_info_t* info)
    {
        return Guard([&] {
            const ffpsd_resolution_info_t& from = Need(info, "info");
            ffpsd::ResolutionInfo value;
            value.horizontal = from.horizontal;
            value.horizontal_unit = from.horizontal_unit;
            value.width_unit = from.width_unit;
            value.vertical = from.vertical;
            value.vertical_unit = from.vertical_unit;
            value.height_unit = from.height_unit;
            Need(doc, "doc").value.SetResolutionInfo(value);
        });
    }

    ffpsd_status_t ffpsd_document_get_version_info(const ffpsd_document_t* doc, ffpsd_version_info_t** out)
    {
        return Guard([&] {
            ffpsd_version_info_t*& target = NeedOut(out);
            target = new ffpsd_version_info_t{Need(doc, "doc").value.GetVersionInfo()};
        });
    }
    ffpsd_status_t ffpsd_document_set_version_info(ffpsd_document_t* doc, const ffpsd_version_info_t* info)
    {
        return Guard([&] { Need(doc, "doc").value.SetVersionInfo(Need(info, "info").value); });
    }

    size_t ffpsd_document_get_image_resource_count(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetImageResourceCount();
    }
    ffpsd_status_t ffpsd_document_get_image_resource_by_index(const ffpsd_document_t* doc, size_t index, ffpsd_image_resource_t* out)
    {
        return Guard([&] {
            ffpsd_image_resource_t& target = Need(out, "out");
            const ffpsd::ImageResource* resource = Need(doc, "doc").value.GetImageResourceByIndex(index);
            Fill(*resource, target);
        });
    }
    ffpsd_status_t ffpsd_document_get_image_resource_by_id(const ffpsd_document_t* doc, uint16_t id, ffpsd_image_resource_t* out)
    {
        return Guard([&] {
            ffpsd_image_resource_t& target = Need(out, "out");
            const ffpsd::ImageResource* resource = Need(doc, "doc").value.GetImageResourceById(id);
            Fill(*FoundOrThrow(resource, "image resource", id), target);
        });
    }
    ffpsd_status_t ffpsd_document_set_image_resource(ffpsd_document_t* doc, const ffpsd_image_resource_t* resource)
    {
        return Guard([&] { Need(doc, "doc").value.SetImageResource(ToResource(Need(resource, "resource"))); });
    }
    ffpsd_status_t ffpsd_document_remove_image_resource(ffpsd_document_t* doc, uint16_t id)
    {
        return Guard([&] {
            if (!Need(doc, "doc").value.RemoveImageResource(id))
                throw NotFound("ffpsd: no image resource " + std::to_string(id));
        });
    }

    size_t ffpsd_document_get_layer_count(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetLayerCount();
    }
    ffpsd_status_t ffpsd_document_get_layer(ffpsd_document_t* doc, size_t index, ffpsd_layer_t** out)
    {
        return Guard([&] {
            ffpsd_layer_t*& target = NeedOut(out);
            target = ToHandle(Need(doc, "doc").value.GetLayerByIndex(index));
        });
    }
    ffpsd_status_t ffpsd_document_add_layer(
        ffpsd_document_t* doc, const char* name, const ffpsd_image_view_t* image, int32_t top, int32_t left, ffpsd_layer_t** out)
    {
        return Guard([&] {
            ffpsd_layer_t*& target = NeedOut(out);
            ffpsd::Document& value = Need(doc, "doc").value;
            const std::string layer_name(&Need(name, "name"));
            target = ToHandle(value.AddLayer(layer_name, image == nullptr ? ffpsd::ImageView() : ToView(*image), top, left));
        });
    }
    ffpsd_status_t
    ffpsd_document_add_background_layer(ffpsd_document_t* doc, const char* name, const ffpsd_image_view_t* image, ffpsd_layer_t** out)
    {
        return Guard([&] {
            ffpsd_layer_t*& target = NeedOut(out);
            ffpsd::Document& value = Need(doc, "doc").value;
            const std::string layer_name(&Need(name, "name"));
            target = ToHandle(value.AddBackgroundLayer(layer_name, ToView(Need(image, "image"))));
        });
    }
    ffpsd_status_t ffpsd_document_set_background_layer(ffpsd_document_t* doc, size_t index)
    {
        return Guard([&] { Need(doc, "doc").value.SetBackgroundLayer(index); });
    }
    ffpsd_status_t ffpsd_document_unset_background_layer(ffpsd_document_t* doc)
    {
        return Guard([&] {
            if (!Need(doc, "doc").value.UnsetBackgroundLayer())
                throw NotFound("ffpsd: the document has no background");
        });
    }
    ffpsd_status_t
    ffpsd_document_add_levels_layer(ffpsd_document_t* doc, const char* name, const ffpsd_levels_t* levels, ffpsd_layer_t** out)
    {
        return Guard([&] {
            ffpsd_layer_t*& target = NeedOut(out);
            const ffpsd::LevelsInfo info = levels == nullptr ? ffpsd::LevelsInfo() : levels->value;
            target = ToHandle(Need(doc, "doc").value.AddAdjustmentLayer(std::string(&Need(name, "name")), info));
        });
    }
    ffpsd_status_t ffpsd_document_add_layer_copy(ffpsd_document_t* doc, const ffpsd_layer_t* source, ffpsd_layer_t** out)
    {
        return Guard([&] {
            ffpsd_layer_t*& target = NeedOut(out);
            target = ToHandle(Need(doc, "doc").value.AddLayerCopy(ToLayer(source)));
        });
    }
    ffpsd_status_t ffpsd_document_remove_layer(ffpsd_document_t* doc, size_t index)
    {
        return Guard([&] { Need(doc, "doc").value.RemoveLayer(index); });
    }
    ffpsd_status_t ffpsd_document_move_layer(ffpsd_document_t* doc, size_t from, size_t to)
    {
        return Guard([&] { Need(doc, "doc").value.MoveLayer(from, to); });
    }

    size_t ffpsd_document_get_tagged_block_count(const ffpsd_document_t* doc)
    {
        return doc == nullptr ? 0 : doc->value.GetTaggedBlockCount();
    }
    ffpsd_status_t ffpsd_document_get_tagged_block_by_index(const ffpsd_document_t* doc, size_t index, ffpsd_tagged_block_t* out)
    {
        return Guard([&] {
            ffpsd_tagged_block_t& target = Need(out, "out");
            Fill(*Need(doc, "doc").value.GetTaggedBlockByIndex(index), target);
        });
    }
    ffpsd_status_t ffpsd_document_get_tagged_block_by_key(const ffpsd_document_t* doc, uint32_t key, ffpsd_tagged_block_t* out)
    {
        return Guard([&] {
            ffpsd_tagged_block_t& target = Need(out, "out");
            const ffpsd::TaggedBlock* block = Need(doc, "doc").value.GetTaggedBlockByKey(key);
            Fill(*FoundOrThrow(block, "tagged block", key), target);
        });
    }
    ffpsd_status_t ffpsd_document_set_tagged_block(ffpsd_document_t* doc, const ffpsd_tagged_block_t* block)
    {
        return Guard([&] { Need(doc, "doc").value.SetTaggedBlock(ToBlock(Need(block, "block"))); });
    }
    ffpsd_status_t ffpsd_document_remove_tagged_block(ffpsd_document_t* doc, uint32_t key)
    {
        return Guard([&] {
            if (!Need(doc, "doc").value.RemoveTaggedBlock(key))
                throw NotFound("ffpsd: no tagged block " + std::to_string(key));
        });
    }

    ffpsd_status_t ffpsd_document_get_merged_image(const ffpsd_document_t* doc, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            target = NewImage(Need(doc, "doc").value.GetMergedImage());
        });
    }
    ffpsd_status_t ffpsd_document_set_merged_image(ffpsd_document_t* doc, const ffpsd_image_view_t* image)
    {
        return Guard([&] { Need(doc, "doc").value.SetMergedImage(ToView(Need(image, "image"))); });
    }

    ffpsd_layer_kind_t ffpsd_layer_get_kind(const ffpsd_layer_t* layer)
    {
        if (layer == nullptr)
            return FFPSD_LAYER_KIND_RASTER;
        return static_cast<ffpsd_layer_kind_t>(reinterpret_cast<const ffpsd::Layer*>(layer)->GetKind());
    }
    ffpsd_status_t ffpsd_layer_get_bounds(const ffpsd_layer_t* layer, ffpsd_rect_t* out)
    {
        return Guard([&] { Fill(ToLayer(layer).GetBounds(), Need(out, "out")); });
    }
    ffpsd_status_t ffpsd_layer_get_name(const ffpsd_layer_t* layer, char* buffer, size_t capacity, size_t* length)
    {
        return Guard([&] {
            const std::string name = ToLayer(layer).GetName();
            if (buffer == nullptr && capacity != 0)
                throw std::invalid_argument("ffpsd: buffer is NULL with a capacity of " + std::to_string(capacity));
            if (capacity != 0)
            {
                const std::size_t copied = std::min(name.size(), capacity - 1);
                std::memcpy(buffer, name.data(), copied);
                buffer[copied] = '\0';
            }
            if (length != nullptr)
                *length = name.size();
        });
    }

    uint8_t ffpsd_layer_get_opacity(const ffpsd_layer_t* layer)
    {
        return layer == nullptr ? 0 : reinterpret_cast<const ffpsd::Layer*>(layer)->GetOpacity();
    }
    int ffpsd_layer_is_visible(const ffpsd_layer_t* layer)
    {
        return layer != nullptr && reinterpret_cast<const ffpsd::Layer*>(layer)->IsVisible() ? 1 : 0;
    }
    int ffpsd_layer_is_background(const ffpsd_layer_t* layer)
    {
        return layer != nullptr && reinterpret_cast<const ffpsd::Layer*>(layer)->IsBackground() ? 1 : 0;
    }
    uint32_t ffpsd_layer_get_blend_key(const ffpsd_layer_t* layer)
    {
        return layer == nullptr ? 0 : reinterpret_cast<const ffpsd::Layer*>(layer)->GetBlendKey();
    }
    ffpsd_status_t ffpsd_layer_set_name(ffpsd_layer_t* layer, const char* name)
    {
        return Guard([&] { ToLayer(layer).SetName(std::string(&Need(name, "name"))); });
    }
    ffpsd_status_t ffpsd_layer_set_opacity(ffpsd_layer_t* layer, uint8_t opacity)
    {
        return Guard([&] { ToLayer(layer).SetOpacity(opacity); });
    }
    ffpsd_status_t ffpsd_layer_set_visible(ffpsd_layer_t* layer, int visible)
    {
        return Guard([&] { ToLayer(layer).SetVisible(visible != 0); });
    }
    ffpsd_status_t ffpsd_layer_set_blend_key(ffpsd_layer_t* layer, uint32_t blend_key)
    {
        return Guard([&] { ToLayer(layer).SetBlendKey(blend_key); });
    }

    uint32_t ffpsd_layer_get_adjustment_key(const ffpsd_layer_t* layer)
    {
        return layer == nullptr ? 0 : reinterpret_cast<const ffpsd::Layer*>(layer)->GetAdjustmentKey();
    }
    ffpsd_status_t ffpsd_layer_get_levels(const ffpsd_layer_t* layer, ffpsd_levels_t** out)
    {
        return Guard([&] {
            ffpsd_levels_t*& target = NeedOut(out);
            std::optional<ffpsd::LevelsInfo> levels = ToLayer(layer).GetAdjustment<ffpsd::LevelsInfo>();
            if (!levels.has_value())
                throw NotFound("ffpsd: the layer is not a Levels layer");
            target = new ffpsd_levels_t{std::move(*levels)};
        });
    }
    ffpsd_status_t ffpsd_layer_set_levels(ffpsd_layer_t* layer, const ffpsd_levels_t* levels)
    {
        return Guard([&] { ToLayer(layer).SetAdjustment(Need(levels, "levels").value); });
    }

    ffpsd_status_t ffpsd_layer_set_position(ffpsd_layer_t* layer, int32_t top, int32_t left)
    {
        return Guard([&] { ToLayer(layer).SetPosition(top, left); });
    }
    ffpsd_status_t ffpsd_layer_resize(ffpsd_layer_t* layer, uint32_t width, uint32_t height, ffpsd_resample_filter_t filter)
    {
        return Guard([&] { ToLayer(layer).Resize(width, height, static_cast<ffpsd::ResampleFilter>(filter)); });
    }

    ffpsd_status_t ffpsd_layer_flip(ffpsd_layer_t* layer, ffpsd_flip_direction_t direction)
    {
        return Guard([&] { ToLayer(layer).Flip(static_cast<ffpsd::FlipDirection>(direction)); });
    }
    ffpsd_status_t ffpsd_layer_rotate(ffpsd_layer_t* layer, ffpsd_rotation_t rotation)
    {
        return Guard([&] { ToLayer(layer).Rotate(static_cast<ffpsd::Rotation>(rotation)); });
    }

    ffpsd_status_t ffpsd_layer_get_pixels(const ffpsd_layer_t* layer, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            target = NewImage(ToLayer(layer).GetPixels());
        });
    }
    ffpsd_status_t ffpsd_layer_set_pixels(ffpsd_layer_t* layer, const ffpsd_image_view_t* image)
    {
        return Guard([&] { ToLayer(layer).SetPixels(ToView(Need(image, "image"))); });
    }

    ffpsd_status_t ffpsd_layer_get_mask(const ffpsd_layer_t* layer, ffpsd_image_t** image, ffpsd_rect_t* bounds, uint8_t* default_color)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(image);
            ffpsd_rect_t& rect = Need(bounds, "bounds");
            std::uint8_t& color = Need(default_color, "default_color");
            std::optional<ffpsd::LayerMask> mask = ToLayer(layer).GetMask();
            if (!mask.has_value())
                throw NotFound("ffpsd: the layer has no pixel mask");
            Fill(mask->bounds, rect);
            color = mask->default_color;
            target = NewImage(std::move(mask->image));
        });
    }
    ffpsd_status_t
    ffpsd_layer_set_mask(ffpsd_layer_t* layer, const ffpsd_image_view_t* image, int32_t top, int32_t left, uint8_t default_color)
    {
        return Guard([&] { ToLayer(layer).SetMask(ToView(Need(image, "image")), top, left, default_color); });
    }
    ffpsd_status_t ffpsd_layer_remove_mask(ffpsd_layer_t* layer)
    {
        return Guard([&] {
            if (!ToLayer(layer).RemoveMask())
                throw NotFound("ffpsd: the layer has no pixel mask");
        });
    }

    size_t ffpsd_layer_get_tagged_block_count(const ffpsd_layer_t* layer)
    {
        return layer == nullptr ? 0 : reinterpret_cast<const ffpsd::Layer*>(layer)->GetTaggedBlockCount();
    }
    ffpsd_status_t ffpsd_layer_get_tagged_block_by_index(const ffpsd_layer_t* layer, size_t index, ffpsd_tagged_block_t* out)
    {
        return Guard([&] {
            ffpsd_tagged_block_t& target = Need(out, "out");
            Fill(*ToLayer(layer).GetTaggedBlockByIndex(index), target);
        });
    }
    ffpsd_status_t ffpsd_layer_get_tagged_block_by_key(const ffpsd_layer_t* layer, uint32_t key, ffpsd_tagged_block_t* out)
    {
        return Guard([&] {
            ffpsd_tagged_block_t& target = Need(out, "out");
            const ffpsd::TaggedBlock* block = ToLayer(layer).GetTaggedBlockByKey(key);
            Fill(*FoundOrThrow(block, "tagged block", key), target);
        });
    }
    ffpsd_status_t ffpsd_layer_set_tagged_block(ffpsd_layer_t* layer, const ffpsd_tagged_block_t* block)
    {
        return Guard([&] { ToLayer(layer).SetTaggedBlock(ToBlock(Need(block, "block"))); });
    }
    ffpsd_status_t ffpsd_layer_remove_tagged_block(ffpsd_layer_t* layer, uint32_t key)
    {
        return Guard([&] {
            if (!ToLayer(layer).RemoveTaggedBlock(key))
                throw NotFound("ffpsd: no tagged block " + std::to_string(key));
        });
    }

    int ffpsd_format_is_supported(ffpsd_format_t format)
    {
        return ffpsd::IsFormatSupported(static_cast<ffpsd::Format>(format)) ? 1 : 0;
    }

    ffpsd_status_t ffpsd_picture_load(const char* path, ffpsd_color_mode_t color_mode, uint16_t depth, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            const std::vector<std::uint8_t> data = ffpsd::detail::ReadFile(std::string(&Need(path, "path")));
            target = NewImage(LoadPicture(data.data(), data.size(), color_mode, depth));
        });
    }
    ffpsd_status_t
    ffpsd_picture_load_memory(const uint8_t* data, size_t size, ffpsd_color_mode_t color_mode, uint16_t depth, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            target = NewImage(LoadPicture(NeedBytes(data, size), size, color_mode, depth));
        });
    }

    ffpsd_status_t ffpsd_png_load(const char* path, ffpsd_color_mode_t color_mode, uint16_t depth, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            RequirePng();
            const std::string file(&Need(path, "path"));
            target = NewImage(LoadPng(file, static_cast<ffpsd::ColorMode>(color_mode), depth));
        });
    }
    ffpsd_status_t
    ffpsd_png_load_memory(const uint8_t* data, size_t size, ffpsd_color_mode_t color_mode, uint16_t depth, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            RequirePng();
            target = NewImage(LoadPng(NeedBytes(data, size), size, static_cast<ffpsd::ColorMode>(color_mode), depth));
        });
    }
    ffpsd_status_t ffpsd_png_save(const ffpsd_image_view_t* image, const char* path)
    {
        return Guard([&] {
            RequirePng();
            SavePng(ToView(Need(image, "image")), std::string(&Need(path, "path")));
        });
    }
    ffpsd_status_t ffpsd_png_save_memory(const ffpsd_image_view_t* image, ffpsd_buffer_t** out)
    {
        return Guard([&] {
            ffpsd_buffer_t*& target = NeedOut(out);
            RequirePng();
            target = new ffpsd_buffer_t{EncodePng(ToView(Need(image, "image")))};
        });
    }

    ffpsd_status_t
    ffpsd_jpeg_load(const char* path, ffpsd_color_mode_t color_mode, uint16_t depth, int apply_orientation, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            RequireJpeg();
            const std::string file(&Need(path, "path"));
            target = NewImage(LoadJpeg(file, static_cast<ffpsd::ColorMode>(color_mode), depth, apply_orientation != 0));
        });
    }
    ffpsd_status_t ffpsd_jpeg_load_memory(
        const uint8_t* data, size_t size, ffpsd_color_mode_t color_mode, uint16_t depth, int apply_orientation, ffpsd_image_t** out)
    {
        return Guard([&] {
            ffpsd_image_t*& target = NeedOut(out);
            RequireJpeg();
            target =
                NewImage(LoadJpeg(NeedBytes(data, size), size, static_cast<ffpsd::ColorMode>(color_mode), depth, apply_orientation != 0));
        });
    }
    ffpsd_status_t ffpsd_jpeg_save(const ffpsd_image_view_t* image, const char* path, int quality)
    {
        return Guard([&] {
            RequireJpeg();
            SaveJpeg(ToView(Need(image, "image")), std::string(&Need(path, "path")), quality);
        });
    }
    ffpsd_status_t ffpsd_jpeg_save_memory(const ffpsd_image_view_t* image, int quality, ffpsd_buffer_t** out)
    {
        return Guard([&] {
            ffpsd_buffer_t*& target = NeedOut(out);
            RequireJpeg();
            target = new ffpsd_buffer_t{EncodeJpeg(ToView(Need(image, "image")), quality)};
        });
    }
} // extern "C"
