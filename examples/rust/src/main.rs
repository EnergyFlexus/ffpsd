//! Opens a PSD, prints what it holds, puts a copy of its top layer on top and saves the result.
mod ffi;

use std::ffi::{CStr, CString};
use std::process::ExitCode;
use std::ptr;

/// The message ffpsd left for the last call on this thread.
fn check(status: ffi::ffpsd_status_t) -> Result<(), String> {
    if status == ffi::FFPSD_STATUS_OK {
        return Ok(());
    }
    // SAFETY: ffpsd_last_error always returns a valid, null-terminated string.
    Err(unsafe { CStr::from_ptr(ffi::ffpsd_last_error()) }.to_string_lossy().into_owned())
}

fn c_path(path: &str) -> Result<CString, String> {
    CString::new(path).map_err(|_| format!("{path}: a path with a null byte"))
}

/// Owns the handle; ffpsd_document_destroy runs when it goes out of scope.
struct Document(*mut ffi::ffpsd_document_t);

/// Borrowed from its document, which it must not outlive.
#[derive(Clone, Copy)]
struct Layer(*mut ffi::ffpsd_layer_t);

impl Document {
    fn open(path: &str) -> Result<Self, String> {
        let path = c_path(path)?;
        let mut doc = ptr::null_mut();
        check(unsafe { ffi::ffpsd_document_open(path.as_ptr(), &mut doc) })?;
        Ok(Document(doc))
    }

    fn save(&self, path: &str) -> Result<(), String> {
        let path = c_path(path)?;
        check(unsafe { ffi::ffpsd_document_save(self.0, path.as_ptr(), ffi::FFPSD_COMPRESSION_RLE) })
    }

    fn layer_count(&self) -> usize {
        unsafe { ffi::ffpsd_document_get_layer_count(self.0) }
    }

    fn layer(&self, index: usize) -> Result<Layer, String> {
        let mut layer = ptr::null_mut();
        check(unsafe { ffi::ffpsd_document_get_layer(self.0, index, &mut layer) })?;
        Ok(Layer(layer))
    }

    fn add_layer_copy(&mut self, source: Layer) -> Result<Layer, String> {
        let mut layer = ptr::null_mut();
        check(unsafe { ffi::ffpsd_document_add_layer_copy(self.0, source.0, &mut layer) })?;
        Ok(Layer(layer))
    }

    fn resolution(&self) -> Result<ffi::ffpsd_resolution_info_t, String> {
        let mut info = ffi::ffpsd_resolution_info_t::default();
        check(unsafe { ffi::ffpsd_document_get_resolution_info(self.0, &mut info) })?;
        Ok(info)
    }

    /// The writer and reader names from resource 1057.
    fn written_by(&self) -> Result<(String, String), String> {
        let mut info = ptr::null_mut();
        check(unsafe { ffi::ffpsd_document_get_version_info(self.0, &mut info) })?;
        // SAFETY: both names live until the handle is destroyed, just below.
        let names = unsafe {
            (
                CStr::from_ptr(ffi::ffpsd_version_info_get_writer_name(info)).to_string_lossy().into_owned(),
                CStr::from_ptr(ffi::ffpsd_version_info_get_reader_name(info)).to_string_lossy().into_owned(),
            )
        };
        unsafe { ffi::ffpsd_version_info_destroy(info) };
        Ok(names)
    }
}

impl Drop for Document {
    fn drop(&mut self) {
        unsafe { ffi::ffpsd_document_destroy(self.0) };
    }
}

impl Layer {
    /// Asks for the length first, then fills a buffer of exactly that size.
    fn name(self) -> Result<String, String> {
        let mut length = 0usize;
        check(unsafe { ffi::ffpsd_layer_get_name(self.0, ptr::null_mut(), 0, &mut length) })?;
        let mut buffer = vec![0u8; length + 1];
        check(unsafe { ffi::ffpsd_layer_get_name(self.0, buffer.as_mut_ptr().cast(), buffer.len(), &mut length) })?;
        buffer.truncate(length);
        Ok(String::from_utf8_lossy(&buffer).into_owned())
    }

    fn kind(self) -> ffi::ffpsd_layer_kind_t {
        unsafe { ffi::ffpsd_layer_get_kind(self.0) }
    }

    fn is_group_marker(self) -> bool {
        let kind = self.kind();
        kind != ffi::FFPSD_LAYER_KIND_RASTER && kind != ffi::FFPSD_LAYER_KIND_ADJUSTMENT
    }

    fn bounds(self) -> Result<ffi::ffpsd_rect_t, String> {
        let mut rect = ffi::ffpsd_rect_t::default();
        check(unsafe { ffi::ffpsd_layer_get_bounds(self.0, &mut rect) })?;
        Ok(rect)
    }

    fn is_visible(self) -> bool {
        unsafe { ffi::ffpsd_layer_is_visible(self.0) != 0 }
    }
}

fn color_name(color: ffi::ffpsd_color_mode_t) -> &'static str {
    match color {
        ffi::FFPSD_COLOR_MODE_GRAYSCALE => "grayscale",
        ffi::FFPSD_COLOR_MODE_RGB => "RGB",
        ffi::FFPSD_COLOR_MODE_CMYK => "CMYK",
        ffi::FFPSD_COLOR_MODE_LAB => "Lab",
        _ => "other",
    }
}

fn kind_name(kind: ffi::ffpsd_layer_kind_t) -> &'static str {
    match kind {
        ffi::FFPSD_LAYER_KIND_RASTER => "raster",
        ffi::FFPSD_LAYER_KIND_ADJUSTMENT => "adjustment",
        ffi::FFPSD_LAYER_KIND_GROUP_END => "group end",
        _ => "group",
    }
}

fn print_document(doc: &Document) -> Result<(), String> {
    let (width, height, depth, color, psb) = unsafe {
        (
            ffi::ffpsd_document_get_width(doc.0),
            ffi::ffpsd_document_get_height(doc.0),
            ffi::ffpsd_document_get_depth(doc.0),
            ffi::ffpsd_document_get_color(doc.0),
            ffi::ffpsd_document_is_psb(doc.0) != 0,
        )
    };
    println!("{width} x {height}, {}, {depth} bit{}", color_name(color), if psb { ", PSB" } else { "" });

    let resolution = doc.resolution()?;
    println!("resolution: {} x {} ppi", resolution.horizontal, resolution.vertical);

    let (writer, reader) = doc.written_by()?;
    println!("written by: {writer}, for {reader}");

    println!("layers, bottom to top: {}", doc.layer_count());
    for index in 0..doc.layer_count() {
        let layer = doc.layer(index)?;
        let bounds = layer.bounds()?;
        println!(
            "  {index}: {} ({}), {} x {} at {}, {}{}",
            layer.name()?,
            kind_name(layer.kind()),
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            bounds.left,
            bounds.top,
            if layer.is_visible() { "" } else { ", hidden" }
        );
    }
    Ok(())
}

fn run(input: &str, output: &str) -> Result<(), String> {
    let mut doc = Document::open(input)?;
    print_document(&doc)?;

    // A copy of one group marker would break the nesting, so the top layer that is not one.
    let mut top = None;
    for index in (0..doc.layer_count()).rev() {
        let layer = doc.layer(index)?;
        if !layer.is_group_marker() {
            top = Some(layer);
            break;
        }
    }
    let top = top.ok_or("no layer to copy")?;

    let name = top.name()?;
    doc.add_layer_copy(top)?;
    doc.save(output)?;
    println!("\ncopied {name} to the top, saved {} layers to {output}", doc.layer_count());
    Ok(())
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        let version = unsafe { CStr::from_ptr(ffi::ffpsd_version()) }.to_string_lossy();
        eprintln!("usage: ffpsd-example <in.psd> <out.psd>    (ffpsd {version})");
        return ExitCode::from(2);
    }

    match run(&args[1], &args[2]) {
        Ok(()) => ExitCode::SUCCESS,
        Err(message) => {
            eprintln!("{message}");
            ExitCode::FAILURE
        }
    }
}
