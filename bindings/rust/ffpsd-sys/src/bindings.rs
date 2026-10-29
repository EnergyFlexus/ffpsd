// Generated from ffpsd/c_api.h by scripts/generate_rust_bindings.py; do not edit.

pub type ffpsd_status_t = ::std::os::raw::c_uint;
pub type ffpsd_color_mode_t = ::std::os::raw::c_uint;
pub type ffpsd_layer_kind_t = ::std::os::raw::c_uint;
pub type ffpsd_compression_t = ::std::os::raw::c_uint;
pub type ffpsd_channel_compression_t = ::std::os::raw::c_uint;
pub type ffpsd_resample_filter_t = ::std::os::raw::c_uint;
pub type ffpsd_anchor_t = ::std::os::raw::c_uint;
pub type ffpsd_rotation_t = ::std::os::raw::c_uint;
pub type ffpsd_flip_direction_t = ::std::os::raw::c_uint;
pub type ffpsd_format_t = ::std::os::raw::c_uint;
pub type ffpsd_png_compression_t = ::std::os::raw::c_uint;
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_document_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_image_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_buffer_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_version_info_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_levels_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_layer_t {
    _unused: [u8; 0],
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_rect_t {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_channel_info_t {
    pub id: i16,
    pub compression: ffpsd_channel_compression_t,
    pub size: u64,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_image_view_t {
    pub width: u32,
    pub height: u32,
    pub channel_count: u16,
    pub depth: u16,
    pub color_mode: ffpsd_color_mode_t,
    pub data: *const u8,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_resolution_info_t {
    pub horizontal: f64,
    pub horizontal_unit: i16,
    pub width_unit: i16,
    pub vertical: f64,
    pub vertical_unit: i16,
    pub height_unit: i16,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_image_resource_t {
    pub id: u16,
    pub name: *const ::std::os::raw::c_char,
    pub data: *const u8,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_tagged_block_t {
    pub signature: u32,
    pub key: u32,
    pub data: *const u8,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ffpsd_levels_channel_t {
    pub input_floor: u16,
    pub input_ceiling: u16,
    pub output_floor: u16,
    pub output_ceiling: u16,
    pub gamma: f64,
}
pub const FFPSD_STATUS_OK: ffpsd_status_t = 0;
pub const FFPSD_STATUS_INVALID_ARGUMENT: ffpsd_status_t = 1;
pub const FFPSD_STATUS_OUT_OF_RANGE: ffpsd_status_t = 2;
pub const FFPSD_STATUS_NOT_FOUND: ffpsd_status_t = 3;
pub const FFPSD_STATUS_INVALID_FILE: ffpsd_status_t = 4;
pub const FFPSD_STATUS_IO: ffpsd_status_t = 5;
pub const FFPSD_STATUS_INVALID_OPERATION: ffpsd_status_t = 6;
pub const FFPSD_STATUS_UNSUPPORTED: ffpsd_status_t = 7;
pub const FFPSD_STATUS_OUT_OF_MEMORY: ffpsd_status_t = 8;
pub const FFPSD_STATUS_UNKNOWN: ffpsd_status_t = 9;
pub const FFPSD_COLOR_MODE_BITMAP: ffpsd_color_mode_t = 0;
pub const FFPSD_COLOR_MODE_GRAYSCALE: ffpsd_color_mode_t = 1;
pub const FFPSD_COLOR_MODE_INDEXED: ffpsd_color_mode_t = 2;
pub const FFPSD_COLOR_MODE_RGB: ffpsd_color_mode_t = 3;
pub const FFPSD_COLOR_MODE_CMYK: ffpsd_color_mode_t = 4;
pub const FFPSD_COLOR_MODE_MULTICHANNEL: ffpsd_color_mode_t = 7;
pub const FFPSD_COLOR_MODE_DUOTONE: ffpsd_color_mode_t = 8;
pub const FFPSD_COLOR_MODE_LAB: ffpsd_color_mode_t = 9;
pub const FFPSD_LAYER_KIND_RASTER: ffpsd_layer_kind_t = 0;
pub const FFPSD_LAYER_KIND_GROUP_OPEN: ffpsd_layer_kind_t = 1;
pub const FFPSD_LAYER_KIND_GROUP_CLOSED: ffpsd_layer_kind_t = 2;
pub const FFPSD_LAYER_KIND_GROUP_END: ffpsd_layer_kind_t = 3;
pub const FFPSD_LAYER_KIND_ADJUSTMENT: ffpsd_layer_kind_t = 4;
pub const FFPSD_COMPRESSION_DEFAULT: ffpsd_compression_t = 0;
pub const FFPSD_COMPRESSION_RAW: ffpsd_compression_t = 1;
pub const FFPSD_COMPRESSION_RLE: ffpsd_compression_t = 2;
pub const FFPSD_COMPRESSION_RLE_OR_RAW: ffpsd_compression_t = 3;
pub const FFPSD_CHANNEL_COMPRESSION_RAW: ffpsd_channel_compression_t = 0;
pub const FFPSD_CHANNEL_COMPRESSION_RLE: ffpsd_channel_compression_t = 1;
pub const FFPSD_CHANNEL_COMPRESSION_ZIP: ffpsd_channel_compression_t = 2;
pub const FFPSD_CHANNEL_COMPRESSION_ZIP_PREDICTION: ffpsd_channel_compression_t = 3;
pub const FFPSD_RESAMPLE_FILTER_NEAREST: ffpsd_resample_filter_t = 0;
pub const FFPSD_RESAMPLE_FILTER_BICUBIC: ffpsd_resample_filter_t = 1;
pub const FFPSD_ANCHOR_TOP_LEFT: ffpsd_anchor_t = 0;
pub const FFPSD_ANCHOR_TOP: ffpsd_anchor_t = 1;
pub const FFPSD_ANCHOR_TOP_RIGHT: ffpsd_anchor_t = 2;
pub const FFPSD_ANCHOR_LEFT: ffpsd_anchor_t = 3;
pub const FFPSD_ANCHOR_CENTER: ffpsd_anchor_t = 4;
pub const FFPSD_ANCHOR_RIGHT: ffpsd_anchor_t = 5;
pub const FFPSD_ANCHOR_BOTTOM_LEFT: ffpsd_anchor_t = 6;
pub const FFPSD_ANCHOR_BOTTOM: ffpsd_anchor_t = 7;
pub const FFPSD_ANCHOR_BOTTOM_RIGHT: ffpsd_anchor_t = 8;
pub const FFPSD_ROTATION_90: ffpsd_rotation_t = 0;
pub const FFPSD_ROTATION_180: ffpsd_rotation_t = 1;
pub const FFPSD_ROTATION_270: ffpsd_rotation_t = 2;
pub const FFPSD_FLIP_HORIZONTAL: ffpsd_flip_direction_t = 0;
pub const FFPSD_FLIP_VERTICAL: ffpsd_flip_direction_t = 1;
pub const FFPSD_FORMAT_PNG: ffpsd_format_t = 0;
pub const FFPSD_FORMAT_JPEG: ffpsd_format_t = 1;
pub const FFPSD_FORMAT_WEBP: ffpsd_format_t = 2;
pub const FFPSD_PNG_COMPRESSION_BALANCED: ffpsd_png_compression_t = 0;
pub const FFPSD_PNG_COMPRESSION_SMALLEST: ffpsd_png_compression_t = 1;
pub const FFPSD_PNG_COMPRESSION_FASTEST: ffpsd_png_compression_t = 2;
extern "C" {
    pub fn ffpsd_version() -> *const ::std::os::raw::c_char;
    pub fn ffpsd_last_error() -> *const ::std::os::raw::c_char;
    pub fn ffpsd_buffer_get_data(buffer: *const ffpsd_buffer_t) -> *const u8;
    pub fn ffpsd_buffer_get_size(buffer: *const ffpsd_buffer_t) -> usize;
    pub fn ffpsd_buffer_destroy(buffer: *mut ffpsd_buffer_t);
    pub fn ffpsd_image_get_view(
        image: *const ffpsd_image_t,
        out: *mut ffpsd_image_view_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_image_destroy(image: *mut ffpsd_image_t);
    pub fn ffpsd_version_info_create(out: *mut *mut ffpsd_version_info_t) -> ffpsd_status_t;
    pub fn ffpsd_version_info_destroy(info: *mut ffpsd_version_info_t);
    pub fn ffpsd_version_info_get_version(info: *const ffpsd_version_info_t) -> u32;
    pub fn ffpsd_version_info_get_has_real_merged_data(
        info: *const ffpsd_version_info_t,
    ) -> ::std::os::raw::c_int;
    pub fn ffpsd_version_info_get_writer_name(
        info: *const ffpsd_version_info_t,
    ) -> *const ::std::os::raw::c_char;
    pub fn ffpsd_version_info_get_reader_name(
        info: *const ffpsd_version_info_t,
    ) -> *const ::std::os::raw::c_char;
    pub fn ffpsd_version_info_get_file_version(info: *const ffpsd_version_info_t) -> u32;
    pub fn ffpsd_version_info_set_version(
        info: *mut ffpsd_version_info_t,
        version: u32,
    ) -> ffpsd_status_t;
    pub fn ffpsd_version_info_set_has_real_merged_data(
        info: *mut ffpsd_version_info_t,
        value: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_version_info_set_writer_name(
        info: *mut ffpsd_version_info_t,
        name: *const ::std::os::raw::c_char,
    ) -> ffpsd_status_t;
    pub fn ffpsd_version_info_set_reader_name(
        info: *mut ffpsd_version_info_t,
        name: *const ::std::os::raw::c_char,
    ) -> ffpsd_status_t;
    pub fn ffpsd_version_info_set_file_version(
        info: *mut ffpsd_version_info_t,
        version: u32,
    ) -> ffpsd_status_t;
    pub fn ffpsd_levels_create(out: *mut *mut ffpsd_levels_t) -> ffpsd_status_t;
    pub fn ffpsd_levels_destroy(levels: *mut ffpsd_levels_t);
    pub fn ffpsd_levels_get_count(levels: *const ffpsd_levels_t) -> usize;
    pub fn ffpsd_levels_get_channel(
        levels: *const ffpsd_levels_t,
        index: usize,
        out: *mut ffpsd_levels_channel_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_levels_set_channel(
        levels: *mut ffpsd_levels_t,
        index: usize,
        channel: *const ffpsd_levels_channel_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_create_with(
        width: u32,
        height: u32,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_document_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_open(
        path: *const ::std::os::raw::c_char,
        out: *mut *mut ffpsd_document_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_open_memory(
        data: *const u8,
        size: usize,
        out: *mut *mut ffpsd_document_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_save(
        doc: *const ffpsd_document_t,
        path: *const ::std::os::raw::c_char,
        compression: ffpsd_compression_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_save_memory(
        doc: *const ffpsd_document_t,
        compression: ffpsd_compression_t,
        out: *mut *mut ffpsd_buffer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_destroy(doc: *mut ffpsd_document_t);
    pub fn ffpsd_document_get_width(doc: *const ffpsd_document_t) -> u32;
    pub fn ffpsd_document_get_height(doc: *const ffpsd_document_t) -> u32;
    pub fn ffpsd_document_get_channel_count(doc: *const ffpsd_document_t) -> u16;
    pub fn ffpsd_document_get_depth(doc: *const ffpsd_document_t) -> u16;
    pub fn ffpsd_document_get_color_mode(doc: *const ffpsd_document_t) -> ffpsd_color_mode_t;
    pub fn ffpsd_document_is_psb(doc: *const ffpsd_document_t) -> ::std::os::raw::c_int;
    pub fn ffpsd_document_get_has_real_merged_data(
        doc: *const ffpsd_document_t,
    ) -> ::std::os::raw::c_int;
    pub fn ffpsd_document_resize_canvas(
        doc: *mut ffpsd_document_t,
        width: u32,
        height: u32,
        anchor: ffpsd_anchor_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_resize(
        doc: *mut ffpsd_document_t,
        width: u32,
        height: u32,
        filter: ffpsd_resample_filter_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_flip_canvas(
        doc: *mut ffpsd_document_t,
        direction: ffpsd_flip_direction_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_rotate_canvas(
        doc: *mut ffpsd_document_t,
        rotation: ffpsd_rotation_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_psb(
        doc: *mut ffpsd_document_t,
        psb: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_convert_color_mode(
        doc: *mut ffpsd_document_t,
        color_mode: ffpsd_color_mode_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_has_real_merged_data(
        doc: *mut ffpsd_document_t,
        value: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_resolution_info(
        doc: *const ffpsd_document_t,
        out: *mut ffpsd_resolution_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_resolution_info(
        doc: *mut ffpsd_document_t,
        info: *const ffpsd_resolution_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_version_info(
        doc: *const ffpsd_document_t,
        out: *mut *mut ffpsd_version_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_version_info(
        doc: *mut ffpsd_document_t,
        info: *const ffpsd_version_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_image_resource_count(doc: *const ffpsd_document_t) -> usize;
    pub fn ffpsd_document_get_image_resource_by_index(
        doc: *const ffpsd_document_t,
        index: usize,
        out: *mut ffpsd_image_resource_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_image_resource_by_id(
        doc: *const ffpsd_document_t,
        id: u16,
        out: *mut ffpsd_image_resource_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_image_resource(
        doc: *mut ffpsd_document_t,
        resource: *const ffpsd_image_resource_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_remove_image_resource(
        doc: *mut ffpsd_document_t,
        id: u16,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_layer_count(doc: *const ffpsd_document_t) -> usize;
    pub fn ffpsd_document_get_layer(
        doc: *mut ffpsd_document_t,
        index: usize,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_find_layer_index(
        doc: *const ffpsd_document_t,
        layer: *const ffpsd_layer_t,
        out: *mut usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_layer(
        doc: *mut ffpsd_document_t,
        name: *const ::std::os::raw::c_char,
        image: *const ffpsd_image_view_t,
        top: i32,
        left: i32,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_background_layer(
        doc: *mut ffpsd_document_t,
        name: *const ::std::os::raw::c_char,
        image: *const ffpsd_image_view_t,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_background_layer(
        doc: *mut ffpsd_document_t,
        index: usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_unset_background_layer(doc: *mut ffpsd_document_t) -> ffpsd_status_t;
    pub fn ffpsd_document_add_group(
        doc: *mut ffpsd_document_t,
        name: *const ::std::os::raw::c_char,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_levels_layer(
        doc: *mut ffpsd_document_t,
        name: *const ::std::os::raw::c_char,
        levels: *const ffpsd_levels_t,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_layer_copy(
        doc: *mut ffpsd_document_t,
        source: *const ffpsd_layer_t,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_remove_layer(doc: *mut ffpsd_document_t, index: usize) -> ffpsd_status_t;
    pub fn ffpsd_document_move_layer(
        doc: *mut ffpsd_document_t,
        from: usize,
        to: usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_tagged_block_count(doc: *const ffpsd_document_t) -> usize;
    pub fn ffpsd_document_get_tagged_block_by_index(
        doc: *const ffpsd_document_t,
        index: usize,
        out: *mut ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_tagged_block_by_key(
        doc: *const ffpsd_document_t,
        key: u32,
        out: *mut ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_tagged_block(
        doc: *mut ffpsd_document_t,
        block: *const ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_remove_tagged_block(
        doc: *mut ffpsd_document_t,
        key: u32,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_merged_image(
        doc: *const ffpsd_document_t,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_merged_image_info(
        doc: *const ffpsd_document_t,
        out: *mut ffpsd_image_view_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_merged_image_bytes(
        doc: *const ffpsd_document_t,
        out: *mut u8,
        size: usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_set_merged_image(
        doc: *mut ffpsd_document_t,
        image: *const ffpsd_image_view_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_merged_compression(
        doc: *const ffpsd_document_t,
        out: *mut ffpsd_channel_compression_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_kind(layer: *const ffpsd_layer_t) -> ffpsd_layer_kind_t;
    pub fn ffpsd_layer_get_bounds(
        layer: *const ffpsd_layer_t,
        out: *mut ffpsd_rect_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_name(
        layer: *const ffpsd_layer_t,
        buffer: *mut ::std::os::raw::c_char,
        capacity: usize,
        length: *mut usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_opacity(layer: *const ffpsd_layer_t) -> u8;
    pub fn ffpsd_layer_is_visible(layer: *const ffpsd_layer_t) -> ::std::os::raw::c_int;
    pub fn ffpsd_layer_is_clipped(layer: *const ffpsd_layer_t) -> ::std::os::raw::c_int;
    pub fn ffpsd_layer_is_background(layer: *const ffpsd_layer_t) -> ::std::os::raw::c_int;
    pub fn ffpsd_layer_get_blend_key(layer: *const ffpsd_layer_t) -> u32;
    pub fn ffpsd_layer_set_name(
        layer: *mut ffpsd_layer_t,
        name: *const ::std::os::raw::c_char,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_opacity(layer: *mut ffpsd_layer_t, opacity: u8) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_visible(
        layer: *mut ffpsd_layer_t,
        visible: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_clipped(
        layer: *mut ffpsd_layer_t,
        clipped: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_group_open(
        layer: *mut ffpsd_layer_t,
        open: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_blend_key(layer: *mut ffpsd_layer_t, blend_key: u32) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_adjustment_key(layer: *const ffpsd_layer_t) -> u32;
    pub fn ffpsd_layer_get_levels(
        layer: *const ffpsd_layer_t,
        out: *mut *mut ffpsd_levels_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_levels(
        layer: *mut ffpsd_layer_t,
        levels: *const ffpsd_levels_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_position(
        layer: *mut ffpsd_layer_t,
        top: i32,
        left: i32,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_resize(
        layer: *mut ffpsd_layer_t,
        width: u32,
        height: u32,
        filter: ffpsd_resample_filter_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_flip(
        layer: *mut ffpsd_layer_t,
        direction: ffpsd_flip_direction_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_rotate(
        layer: *mut ffpsd_layer_t,
        rotation: ffpsd_rotation_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_pixels(
        layer: *const ffpsd_layer_t,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_pixels_info(
        layer: *const ffpsd_layer_t,
        out: *mut ffpsd_image_view_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_pixels_bytes(
        layer: *const ffpsd_layer_t,
        out: *mut u8,
        size: usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_pixels(
        layer: *mut ffpsd_layer_t,
        image: *const ffpsd_image_view_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_mask(
        layer: *const ffpsd_layer_t,
        image: *mut *mut ffpsd_image_t,
        bounds: *mut ffpsd_rect_t,
        default_color: *mut u8,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_mask(
        layer: *mut ffpsd_layer_t,
        image: *const ffpsd_image_view_t,
        top: i32,
        left: i32,
        default_color: u8,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_remove_mask(layer: *mut ffpsd_layer_t) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_channel_count(layer: *const ffpsd_layer_t) -> usize;
    pub fn ffpsd_layer_get_channel_by_index(
        layer: *const ffpsd_layer_t,
        index: usize,
        out: *mut ffpsd_channel_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_tagged_block_count(layer: *const ffpsd_layer_t) -> usize;
    pub fn ffpsd_layer_get_tagged_block_by_index(
        layer: *const ffpsd_layer_t,
        index: usize,
        out: *mut ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_tagged_block_by_key(
        layer: *const ffpsd_layer_t,
        key: u32,
        out: *mut ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_set_tagged_block(
        layer: *mut ffpsd_layer_t,
        block: *const ffpsd_tagged_block_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_remove_tagged_block(layer: *mut ffpsd_layer_t, key: u32) -> ffpsd_status_t;
    pub fn ffpsd_format_is_supported(format: ffpsd_format_t) -> ::std::os::raw::c_int;
    pub fn ffpsd_picture_load(
        path: *const ::std::os::raw::c_char,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_picture_load_memory(
        data: *const u8,
        size: usize,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_png_load(
        path: *const ::std::os::raw::c_char,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_png_load_memory(
        data: *const u8,
        size: usize,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_png_save(
        image: *const ffpsd_image_view_t,
        path: *const ::std::os::raw::c_char,
        compression: ffpsd_png_compression_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_png_save_memory(
        image: *const ffpsd_image_view_t,
        compression: ffpsd_png_compression_t,
        out: *mut *mut ffpsd_buffer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_jpeg_load(
        path: *const ::std::os::raw::c_char,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        apply_orientation: ::std::os::raw::c_int,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_jpeg_load_memory(
        data: *const u8,
        size: usize,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        apply_orientation: ::std::os::raw::c_int,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_jpeg_save(
        image: *const ffpsd_image_view_t,
        path: *const ::std::os::raw::c_char,
        quality: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_jpeg_save_memory(
        image: *const ffpsd_image_view_t,
        quality: ::std::os::raw::c_int,
        out: *mut *mut ffpsd_buffer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_webp_load(
        path: *const ::std::os::raw::c_char,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_webp_load_memory(
        data: *const u8,
        size: usize,
        color_mode: ffpsd_color_mode_t,
        depth: u16,
        out: *mut *mut ffpsd_image_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_webp_save(
        image: *const ffpsd_image_view_t,
        path: *const ::std::os::raw::c_char,
        quality: ::std::os::raw::c_int,
        lossless: ::std::os::raw::c_int,
    ) -> ffpsd_status_t;
    pub fn ffpsd_webp_save_memory(
        image: *const ffpsd_image_view_t,
        quality: ::std::os::raw::c_int,
        lossless: ::std::os::raw::c_int,
        out: *mut *mut ffpsd_buffer_t,
    ) -> ffpsd_status_t;
}
