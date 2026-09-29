//! Opens a PSD, prints what it holds, copies the top layer and saves the result.
mod ffi;

use std::ffi::{CStr, CString};
use std::process::ExitCode;
use std::ptr;

/// Turns a failed status into the message ffpsd left for it.
fn check(status: ffi::ffpsd_status_t) -> Result<(), String> {
    if status == ffi::FFPSD_STATUS_OK {
        return Ok(());
    }
    // SAFETY: ffpsd_last_error always returns a valid, null-terminated string.
    Err(unsafe { CStr::from_ptr(ffi::ffpsd_last_error()) }.to_string_lossy().into_owned())
}

/// Owns the handle: the document, with its layers, is destroyed when this goes out of scope.
struct Document(*mut ffi::ffpsd_document_t);

impl Drop for Document {
    fn drop(&mut self) {
        unsafe { ffi::ffpsd_document_destroy(self.0) };
    }
}

/// Asks for the length first, then fills a buffer of exactly that size.
fn layer_name(layer: *const ffi::ffpsd_layer_t) -> Result<String, String> {
    let mut length = 0usize;
    check(unsafe { ffi::ffpsd_layer_get_name(layer, ptr::null_mut(), 0, &mut length) })?;
    let mut buffer = vec![0u8; length + 1];
    check(unsafe { ffi::ffpsd_layer_get_name(layer, buffer.as_mut_ptr().cast(), buffer.len(), &mut length) })?;
    buffer.truncate(length);
    Ok(String::from_utf8_lossy(&buffer).into_owned())
}

fn run(input: &str, output: &str) -> Result<(), String> {
    let input_path = CString::new(input).map_err(|_| format!("{input}: a path with a null byte"))?;
    let output_path = CString::new(output).map_err(|_| format!("{output}: a path with a null byte"))?;

    // Open the document.
    let mut handle = ptr::null_mut();
    check(unsafe { ffi::ffpsd_document_open(input_path.as_ptr(), &mut handle) })?;
    let doc = Document(handle);

    // Read what it holds; layers go from the bottom up.
    let (width, height, depth, count) = unsafe {
        (
            ffi::ffpsd_document_get_width(doc.0),
            ffi::ffpsd_document_get_height(doc.0),
            ffi::ffpsd_document_get_depth(doc.0),
            ffi::ffpsd_document_get_layer_count(doc.0),
        )
    };
    println!("{width} x {height}, {depth} bit, {count} layers");
    for index in 0..count {
        let mut layer = ptr::null_mut();
        check(unsafe { ffi::ffpsd_document_get_layer(doc.0, index, &mut layer) })?;
        let mut bounds = ffi::ffpsd_rect_t::default();
        check(unsafe { ffi::ffpsd_layer_get_bounds(layer, &mut bounds) })?;
        println!("  {index}: {}, {} x {}", layer_name(layer)?, bounds.right - bounds.left, bounds.bottom - bounds.top);
    }

    // Put a copy of the top layer on top; a group marker alone cannot be copied.
    let mut top = ptr::null_mut();
    check(unsafe { ffi::ffpsd_document_get_layer(doc.0, count.wrapping_sub(1), &mut top) })?;
    let mut copy = ptr::null_mut();
    check(unsafe { ffi::ffpsd_document_add_layer_copy(doc.0, top, &mut copy) })?;

    // Save, packed with RLE as Photoshop does.
    check(unsafe { ffi::ffpsd_document_save(doc.0, output_path.as_ptr(), ffi::FFPSD_COMPRESSION_RLE) })?;
    println!("saved {} layers to {output}", unsafe { ffi::ffpsd_document_get_layer_count(doc.0) });
    Ok(())
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: ffpsd-example <in.psd> <out.psd>");
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
