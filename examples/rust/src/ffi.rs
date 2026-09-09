//! The part of ffpsd/c_api.h this example calls, declared by hand.
#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int};

// Opaque handles: only ever behind a pointer.
#[repr(C)]
pub struct ffpsd_document_t {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ffpsd_layer_t {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ffpsd_version_info_t {
    _private: [u8; 0],
}

// C enums are int-sized on every platform ffpsd builds for.
pub type ffpsd_status_t = c_int;
pub type ffpsd_color_mode_t = c_int;
pub type ffpsd_layer_kind_t = c_int;

pub const FFPSD_STATUS_OK: ffpsd_status_t = 0;

pub type ffpsd_compression_t = c_int;
pub const FFPSD_COMPRESSION_RLE: ffpsd_compression_t = 1;

pub const FFPSD_COLOR_MODE_GRAYSCALE: ffpsd_color_mode_t = 1;
pub const FFPSD_COLOR_MODE_RGB: ffpsd_color_mode_t = 3;
pub const FFPSD_COLOR_MODE_CMYK: ffpsd_color_mode_t = 4;
pub const FFPSD_COLOR_MODE_LAB: ffpsd_color_mode_t = 9;

pub const FFPSD_LAYER_KIND_RASTER: ffpsd_layer_kind_t = 0;
pub const FFPSD_LAYER_KIND_GROUP_END: ffpsd_layer_kind_t = 3;
pub const FFPSD_LAYER_KIND_ADJUSTMENT: ffpsd_layer_kind_t = 4;

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct ffpsd_rect_t {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct ffpsd_resolution_info_t {
    pub horizontal: f64,
    pub horizontal_unit: i16,
    pub width_unit: i16,
    pub vertical: f64,
    pub vertical_unit: i16,
    pub height_unit: i16,
}

extern "C" {
    pub fn ffpsd_version() -> *const c_char;
    pub fn ffpsd_last_error() -> *const c_char;

    pub fn ffpsd_document_open(path: *const c_char, out: *mut *mut ffpsd_document_t) -> ffpsd_status_t;
    pub fn ffpsd_document_save(
        doc: *const ffpsd_document_t,
        path: *const c_char,
        compression: ffpsd_compression_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_destroy(doc: *mut ffpsd_document_t);

    pub fn ffpsd_document_get_width(doc: *const ffpsd_document_t) -> u32;
    pub fn ffpsd_document_get_height(doc: *const ffpsd_document_t) -> u32;
    pub fn ffpsd_document_get_depth(doc: *const ffpsd_document_t) -> u16;
    pub fn ffpsd_document_get_color(doc: *const ffpsd_document_t) -> ffpsd_color_mode_t;
    pub fn ffpsd_document_is_psb(doc: *const ffpsd_document_t) -> c_int;

    pub fn ffpsd_document_get_resolution_info(
        doc: *const ffpsd_document_t,
        out: *mut ffpsd_resolution_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_get_version_info(
        doc: *const ffpsd_document_t,
        out: *mut *mut ffpsd_version_info_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_version_info_get_writer_name(info: *const ffpsd_version_info_t) -> *const c_char;
    pub fn ffpsd_version_info_get_reader_name(info: *const ffpsd_version_info_t) -> *const c_char;
    pub fn ffpsd_version_info_destroy(info: *mut ffpsd_version_info_t);

    pub fn ffpsd_document_get_layer_count(doc: *const ffpsd_document_t) -> usize;
    pub fn ffpsd_document_get_layer(
        doc: *mut ffpsd_document_t,
        index: usize,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_layer_copy(
        doc: *mut ffpsd_document_t,
        source: *const ffpsd_layer_t,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;

    pub fn ffpsd_layer_get_kind(layer: *const ffpsd_layer_t) -> ffpsd_layer_kind_t;
    pub fn ffpsd_layer_get_bounds(layer: *const ffpsd_layer_t, out: *mut ffpsd_rect_t) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_name(
        layer: *const ffpsd_layer_t,
        buffer: *mut c_char,
        capacity: usize,
        length: *mut usize,
    ) -> ffpsd_status_t;
    pub fn ffpsd_layer_is_visible(layer: *const ffpsd_layer_t) -> c_int;
}
