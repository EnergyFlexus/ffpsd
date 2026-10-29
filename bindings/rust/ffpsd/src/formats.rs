//! PNG, JPEG and WebP: loaded into the planes a layer takes, saved from them. A format this build of ffpsd
//! lacks fails with ErrorKind::Unsupported.
use crate::error::{c_path, check, Result};
use crate::types::{take_buffer, ColorMode, Format, Image, ImageView, PngCompression};
use ffpsd_sys as sys;
use std::path::Path;
use std::ptr;

/// Whether this build of ffpsd reads and writes the format.
pub fn is_format_supported(format: Format) -> bool {
    unsafe { sys::ffpsd_format_is_supported(format.to_raw()) != 0 }
}

fn load(load: impl FnOnce(&mut *mut sys::ffpsd_image_t) -> sys::ffpsd_status_t) -> Result<Image> {
    let mut image = ptr::null_mut();
    check(load(&mut image))?;
    Image::take(image)
}

fn save_to_memory(save: impl FnOnce(&mut *mut sys::ffpsd_buffer_t) -> sys::ffpsd_status_t) -> Result<Vec<u8>> {
    let mut buffer = ptr::null_mut();
    check(save(&mut buffer))?;
    Ok(take_buffer(buffer))
}

/// PNG, JPEG or WebP by the signature, a JPEG turned upright by its EXIF.
pub fn load_picture(path: impl AsRef<Path>, color_mode: ColorMode, depth: u16) -> Result<Image> {
    let path = c_path(path.as_ref())?;
    load(|out| unsafe { sys::ffpsd_picture_load(path.as_ptr(), color_mode.to_raw(), depth, out) })
}

pub fn load_picture_from_memory(data: &[u8], color_mode: ColorMode, depth: u16) -> Result<Image> {
    load(|out| unsafe { sys::ffpsd_picture_load_memory(data.as_ptr(), data.len(), color_mode.to_raw(), depth, out) })
}

pub fn load_png(path: impl AsRef<Path>, color_mode: ColorMode, depth: u16) -> Result<Image> {
    let path = c_path(path.as_ref())?;
    load(|out| unsafe { sys::ffpsd_png_load(path.as_ptr(), color_mode.to_raw(), depth, out) })
}

pub fn load_png_from_memory(data: &[u8], color_mode: ColorMode, depth: u16) -> Result<Image> {
    load(|out| unsafe { sys::ffpsd_png_load_memory(data.as_ptr(), data.len(), color_mode.to_raw(), depth, out) })
}

/// Gray, gray with alpha, RGB or RGBA by the channel count; 8 or 16 bit.
pub fn save_png(image: ImageView<'_>, path: impl AsRef<Path>, compression: PngCompression) -> Result<()> {
    let path = c_path(path.as_ref())?;
    let image = image.to_raw();
    check(unsafe { sys::ffpsd_png_save(&image, path.as_ptr(), compression.to_raw()) })
}

pub fn save_png_to_memory(image: ImageView<'_>, compression: PngCompression) -> Result<Vec<u8>> {
    let image = image.to_raw();
    save_to_memory(|out| unsafe { sys::ffpsd_png_save_memory(&image, compression.to_raw(), out) })
}

/// apply_orientation turns the pixels upright by the EXIF orientation.
pub fn load_jpeg(path: impl AsRef<Path>, color_mode: ColorMode, depth: u16, apply_orientation: bool) -> Result<Image> {
    let path = c_path(path.as_ref())?;
    load(|out| unsafe {
        sys::ffpsd_jpeg_load(path.as_ptr(), color_mode.to_raw(), depth, apply_orientation.into(), out)
    })
}

pub fn load_jpeg_from_memory(data: &[u8], color_mode: ColorMode, depth: u16, apply_orientation: bool) -> Result<Image> {
    load(|out| unsafe {
        sys::ffpsd_jpeg_load_memory(
            data.as_ptr(),
            data.len(),
            color_mode.to_raw(),
            depth,
            apply_orientation.into(),
            out,
        )
    })
}

/// Gray from 1 or 2 channels, RGB from 3 or 4; alpha is dropped and 16 bit narrowed to 8; quality is 1 to 100.
pub fn save_jpeg(image: ImageView<'_>, path: impl AsRef<Path>, quality: u8) -> Result<()> {
    let path = c_path(path.as_ref())?;
    let image = image.to_raw();
    check(unsafe { sys::ffpsd_jpeg_save(&image, path.as_ptr(), quality.into()) })
}

pub fn save_jpeg_to_memory(image: ImageView<'_>, quality: u8) -> Result<Vec<u8>> {
    let image = image.to_raw();
    save_to_memory(|out| unsafe { sys::ffpsd_jpeg_save_memory(&image, quality.into(), out) })
}

/// An animated WebP is refused.
pub fn load_webp(path: impl AsRef<Path>, color_mode: ColorMode, depth: u16) -> Result<Image> {
    let path = c_path(path.as_ref())?;
    load(|out| unsafe { sys::ffpsd_webp_load(path.as_ptr(), color_mode.to_raw(), depth, out) })
}

pub fn load_webp_from_memory(data: &[u8], color_mode: ColorMode, depth: u16) -> Result<Image> {
    load(|out| unsafe { sys::ffpsd_webp_load_memory(data.as_ptr(), data.len(), color_mode.to_raw(), depth, out) })
}

/// RGBA when a pixel is not opaque, RGB otherwise; gray to the colors, 16 bit to 8; quality 0 to 100, the effort
/// when lossless.
pub fn save_webp(image: ImageView<'_>, path: impl AsRef<Path>, quality: u8, lossless: bool) -> Result<()> {
    let path = c_path(path.as_ref())?;
    let image = image.to_raw();
    check(unsafe { sys::ffpsd_webp_save(&image, path.as_ptr(), quality.into(), lossless.into()) })
}

pub fn save_webp_to_memory(image: ImageView<'_>, quality: u8, lossless: bool) -> Result<Vec<u8>> {
    let image = image.to_raw();
    save_to_memory(|out| unsafe { sys::ffpsd_webp_save_memory(&image, quality.into(), lossless.into(), out) })
}
