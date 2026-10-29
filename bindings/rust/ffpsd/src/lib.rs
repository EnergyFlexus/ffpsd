//! Rust bindings to ffpsd, a library that reads and writes Photoshop PSD and PSB files.
//!
//! ```no_run
//! use ffpsd::{Compression, Document};
//!
//! let mut document = Document::open("in.psd")?;
//! for layer in document.layers() {
//!     println!("{}: {:?}", layer.name(), layer.bounds());
//! }
//! document.duplicate_layer(document.layer_count() - 1)?.set_name("Copy")?;
//! document.save("out.psd", Compression::RleOrRaw)?;
//! # Ok::<(), ffpsd::Error>(())
//! ```

mod document;
mod error;
pub mod formats;
mod layer;
mod types;

pub use document::{Document, Layers};
pub use error::{Error, ErrorKind, Result};
pub use layer::{Layer, LayerMut};
pub use types::{
    fourcc, Anchor, ChannelCompression, ChannelInfo, ColorMode, Compression, FlipDirection, Format, Image, ImageInfo,
    ImageResource, ImageView, LayerKind, Levels, LevelsChannel, Mask, PngCompression, Rect, ResampleFilter,
    ResolutionInfo, Rotation, TaggedBlock, VersionInfo,
};

/// ffpsd's version, such as "0.4.4".
pub fn version() -> &'static str {
    // SAFETY: a static, null-terminated ASCII string.
    unsafe { std::ffi::CStr::from_ptr(ffpsd_sys::ffpsd_version()) }
        .to_str()
        .unwrap_or_default()
}
