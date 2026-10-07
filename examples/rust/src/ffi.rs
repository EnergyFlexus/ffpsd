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

// C enums are int-sized on every platform ffpsd builds for.
pub type ffpsd_status_t = c_int;
pub type ffpsd_compression_t = c_int;
pub type ffpsd_color_mode_t = c_int;

pub const FFPSD_STATUS_OK: ffpsd_status_t = 0;
pub const FFPSD_COMPRESSION_RLE_OR_RAW: ffpsd_compression_t = 3;
pub const FFPSD_COLOR_MODE_GRAYSCALE: ffpsd_color_mode_t = 1;

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct ffpsd_rect_t {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}

// Planar samples in native byte order, borrowed for the call; the *_info calls leave data null and size the bytes needed.
#[repr(C)]
pub struct ffpsd_image_view_t {
    pub width: u32,
    pub height: u32,
    pub channel_count: u16,
    pub depth: u16,
    pub color_mode: ffpsd_color_mode_t,
    pub data: *const u8,
    pub size: usize,
}

impl Default for ffpsd_image_view_t {
    fn default() -> Self {
        Self {
            width: 0,
            height: 0,
            channel_count: 0,
            depth: 0,
            color_mode: 0,
            data: std::ptr::null(),
            size: 0,
        }
    }
}

extern "C" {
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
    pub fn ffpsd_document_get_color_mode(doc: *const ffpsd_document_t) -> ffpsd_color_mode_t;

    pub fn ffpsd_document_get_layer_count(doc: *const ffpsd_document_t) -> usize;
    pub fn ffpsd_document_get_layer(
        doc: *mut ffpsd_document_t,
        index: usize,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_layer(
        doc: *mut ffpsd_document_t,
        name: *const c_char,
        image: *const ffpsd_image_view_t,
        top: i32,
        left: i32,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;
    pub fn ffpsd_document_add_layer_copy(
        doc: *mut ffpsd_document_t,
        source: *const ffpsd_layer_t,
        out: *mut *mut ffpsd_layer_t,
    ) -> ffpsd_status_t;

    pub fn ffpsd_layer_get_bounds(layer: *const ffpsd_layer_t, out: *mut ffpsd_rect_t) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_pixels_info(layer: *const ffpsd_layer_t, out: *mut ffpsd_image_view_t) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_pixels_bytes(layer: *const ffpsd_layer_t, out: *mut u8, size: usize) -> ffpsd_status_t;
    pub fn ffpsd_layer_get_name(
        layer: *const ffpsd_layer_t,
        buffer: *mut c_char,
        capacity: usize,
        length: *mut usize,
    ) -> ffpsd_status_t;
}
