use crate::error::{c_string, check, Error, ErrorKind, Result};
use ffpsd_sys as sys;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr;
use std::slice;

/// A Rust enum over a C one, both ways; some enums only go one of them.
macro_rules! c_enum {
    ($(#[$meta:meta])* $name:ident: $raw:ty { $($(#[$variant_meta:meta])* $variant:ident = $constant:ident),+ $(,)? }) => {
        $(#[$meta])*
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum $name {
            $($(#[$variant_meta])* $variant),+
        }

        #[allow(dead_code)]
        impl $name {
            pub(crate) fn to_raw(self) -> $raw {
                match self {
                    $(Self::$variant => sys::$constant),+
                }
            }

            pub(crate) fn from_raw(raw: $raw) -> Result<Self> {
                match raw {
                    $(sys::$constant => Ok(Self::$variant),)+
                    _ => Err(Error::new(ErrorKind::Unknown, format!(concat!(stringify!($name), " {} is unknown"), raw))),
                }
            }
        }
    };
}

c_enum! {
    ColorMode: sys::ffpsd_color_mode_t {
        Bitmap = FFPSD_COLOR_MODE_BITMAP,
        Grayscale = FFPSD_COLOR_MODE_GRAYSCALE,
        Indexed = FFPSD_COLOR_MODE_INDEXED,
        Rgb = FFPSD_COLOR_MODE_RGB,
        Cmyk = FFPSD_COLOR_MODE_CMYK,
        Multichannel = FFPSD_COLOR_MODE_MULTICHANNEL,
        Duotone = FFPSD_COLOR_MODE_DUOTONE,
        Lab = FFPSD_COLOR_MODE_LAB,
    }
}

c_enum! {
    /// A group takes two records; stored bottom to top, its end marker comes first.
    LayerKind: sys::ffpsd_layer_kind_t {
        Raster = FFPSD_LAYER_KIND_RASTER,
        GroupOpen = FFPSD_LAYER_KIND_GROUP_OPEN,
        GroupClosed = FFPSD_LAYER_KIND_GROUP_CLOSED,
        GroupEnd = FFPSD_LAYER_KIND_GROUP_END,
        Adjustment = FFPSD_LAYER_KIND_ADJUSTMENT,
    }
}

c_enum! {
    /// How a save writes pixel data.
    #[derive(Default)]
    Compression: sys::ffpsd_compression_t {
        /// Each channel as stored; pixels set through ffpsd are RleOrRaw.
        #[default]
        Default = FFPSD_COMPRESSION_DEFAULT,
        Raw = FFPSD_COMPRESSION_RAW,
        Rle = FFPSD_COMPRESSION_RLE,
        RleOrRaw = FFPSD_COMPRESSION_RLE_OR_RAW,
    }
}

c_enum! {
    /// A channel's compression as the file stores it.
    ChannelCompression: sys::ffpsd_channel_compression_t {
        Raw = FFPSD_CHANNEL_COMPRESSION_RAW,
        Rle = FFPSD_CHANNEL_COMPRESSION_RLE,
        Zip = FFPSD_CHANNEL_COMPRESSION_ZIP,
        ZipPrediction = FFPSD_CHANNEL_COMPRESSION_ZIP_PREDICTION,
    }
}

c_enum! {
    ResampleFilter: sys::ffpsd_resample_filter_t {
        Nearest = FFPSD_RESAMPLE_FILTER_NEAREST,
        Bicubic = FFPSD_RESAMPLE_FILTER_BICUBIC,
    }
}

c_enum! {
    /// Where the old canvas stays when the canvas is resized.
    Anchor: sys::ffpsd_anchor_t {
        TopLeft = FFPSD_ANCHOR_TOP_LEFT,
        Top = FFPSD_ANCHOR_TOP,
        TopRight = FFPSD_ANCHOR_TOP_RIGHT,
        Left = FFPSD_ANCHOR_LEFT,
        Center = FFPSD_ANCHOR_CENTER,
        Right = FFPSD_ANCHOR_RIGHT,
        BottomLeft = FFPSD_ANCHOR_BOTTOM_LEFT,
        Bottom = FFPSD_ANCHOR_BOTTOM,
        BottomRight = FFPSD_ANCHOR_BOTTOM_RIGHT,
    }
}

c_enum! {
    /// Clockwise.
    Rotation: sys::ffpsd_rotation_t {
        Rotate90 = FFPSD_ROTATION_90,
        Rotate180 = FFPSD_ROTATION_180,
        Rotate270 = FFPSD_ROTATION_270,
    }
}

c_enum! {
    FlipDirection: sys::ffpsd_flip_direction_t {
        /// Mirrors left and right.
        Horizontal = FFPSD_FLIP_HORIZONTAL,
        Vertical = FFPSD_FLIP_VERTICAL,
    }
}

c_enum! {
    Format: sys::ffpsd_format_t {
        Png = FFPSD_FORMAT_PNG,
        Jpeg = FFPSD_FORMAT_JPEG,
        Webp = FFPSD_FORMAT_WEBP,
    }
}

c_enum! {
    /// Speed against size of a written PNG; Fastest can grow flat pictures a few times over.
    #[derive(Default)]
    PngCompression: sys::ffpsd_png_compression_t {
        #[default]
        Balanced = FFPSD_PNG_COMPRESSION_BALANCED,
        Smallest = FFPSD_PNG_COMPRESSION_SMALLEST,
        Fastest = FFPSD_PNG_COMPRESSION_FASTEST,
    }
}

/// A four-character code as ffpsd keeps it, such as `fourcc(b"levl")` for a Levels block.
pub const fn fourcc(code: &[u8; 4]) -> u32 {
    u32::from_be_bytes(*code)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Rect {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}

impl Rect {
    pub fn width(&self) -> u32 {
        (i64::from(self.right) - i64::from(self.left)).max(0) as u32
    }

    pub fn height(&self) -> u32 {
        (i64::from(self.bottom) - i64::from(self.top)).max(0) as u32
    }

    pub(crate) fn from_raw(raw: sys::ffpsd_rect_t) -> Self {
        Self {
            top: raw.top,
            left: raw.left,
            bottom: raw.bottom,
            right: raw.right,
        }
    }
}

/// The shape of planar pixels: channel after channel, samples in native byte order.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ImageInfo {
    pub width: u32,
    pub height: u32,
    pub channel_count: u16,
    pub depth: u16,
    pub color_mode: ColorMode,
}

impl ImageInfo {
    pub(crate) fn from_raw(raw: &sys::ffpsd_image_view_t) -> Result<Self> {
        Ok(Self {
            width: raw.width,
            height: raw.height,
            channel_count: raw.channel_count,
            depth: raw.depth,
            color_mode: ColorMode::from_raw(raw.color_mode)?,
        })
    }
}

/// Pixels borrowed for one call, as ffpsd takes them.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ImageView<'a> {
    pub info: ImageInfo,
    pub data: &'a [u8],
}

impl<'a> ImageView<'a> {
    pub fn new(info: ImageInfo, data: &'a [u8]) -> Self {
        Self { info, data }
    }

    pub(crate) fn to_raw(self) -> sys::ffpsd_image_view_t {
        sys::ffpsd_image_view_t {
            width: self.info.width,
            height: self.info.height,
            channel_count: self.info.channel_count,
            depth: self.info.depth,
            color_mode: self.info.color_mode.to_raw(),
            data: self.data.as_ptr(),
            size: self.data.len(),
        }
    }
}

/// Pixels that belong to the caller.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Image {
    pub info: ImageInfo,
    pub data: Vec<u8>,
}

impl Image {
    pub fn view(&self) -> ImageView<'_> {
        ImageView::new(self.info, &self.data)
    }

    /// Copies an image ffpsd made and destroys it.
    pub(crate) fn take(image: *mut sys::ffpsd_image_t) -> Result<Self> {
        let image = Owned::new(image, sys::ffpsd_image_destroy);
        let mut view = empty_view();
        check(unsafe { sys::ffpsd_image_get_view(image.ptr, &mut view) })?;
        let data = unsafe { bytes(view.data, view.size) }.to_vec();
        Ok(Self {
            info: ImageInfo::from_raw(&view)?,
            data,
        })
    }

    /// The shape first, then the bytes straight into a Vec: one decode, no copy.
    pub(crate) fn read(
        info: impl FnOnce(&mut sys::ffpsd_image_view_t) -> sys::ffpsd_status_t,
        bytes: impl FnOnce(*mut u8, usize) -> sys::ffpsd_status_t,
    ) -> Result<Self> {
        let mut view = empty_view();
        check(info(&mut view))?;
        let mut data = vec![0u8; view.size];
        check(bytes(data.as_mut_ptr(), data.len()))?;
        Ok(Self {
            info: ImageInfo::from_raw(&view)?,
            data,
        })
    }
}

/// Resource 1005; resolutions in pixels per inch.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ResolutionInfo {
    pub horizontal: f64,
    pub horizontal_unit: i16,
    pub width_unit: i16,
    pub vertical: f64,
    pub vertical_unit: i16,
    pub height_unit: i16,
}

impl ResolutionInfo {
    pub(crate) fn from_raw(raw: &sys::ffpsd_resolution_info_t) -> Self {
        Self {
            horizontal: raw.horizontal,
            horizontal_unit: raw.horizontal_unit,
            width_unit: raw.width_unit,
            vertical: raw.vertical,
            vertical_unit: raw.vertical_unit,
            height_unit: raw.height_unit,
        }
    }

    pub(crate) fn to_raw(self) -> sys::ffpsd_resolution_info_t {
        sys::ffpsd_resolution_info_t {
            horizontal: self.horizontal,
            horizontal_unit: self.horizontal_unit,
            width_unit: self.width_unit,
            vertical: self.vertical,
            vertical_unit: self.vertical_unit,
            height_unit: self.height_unit,
        }
    }
}

/// Resource 1057.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VersionInfo {
    pub version: u32,
    pub has_real_merged_data: bool,
    pub writer_name: String,
    pub reader_name: String,
    pub file_version: u32,
}

impl VersionInfo {
    pub(crate) fn take(info: *mut sys::ffpsd_version_info_t) -> Self {
        let info = Owned::new(info, sys::ffpsd_version_info_destroy);
        unsafe {
            Self {
                version: sys::ffpsd_version_info_get_version(info.ptr),
                has_real_merged_data: sys::ffpsd_version_info_get_has_real_merged_data(info.ptr) != 0,
                writer_name: string(sys::ffpsd_version_info_get_writer_name(info.ptr)),
                reader_name: string(sys::ffpsd_version_info_get_reader_name(info.ptr)),
                file_version: sys::ffpsd_version_info_get_file_version(info.ptr),
            }
        }
    }

    pub(crate) fn to_raw(&self) -> Result<Owned<sys::ffpsd_version_info_t>> {
        let mut info = ptr::null_mut();
        check(unsafe { sys::ffpsd_version_info_create(&mut info) })?;
        let info = Owned::new(info, sys::ffpsd_version_info_destroy);
        let writer_name = c_string(&self.writer_name)?;
        let reader_name = c_string(&self.reader_name)?;
        unsafe {
            check(sys::ffpsd_version_info_set_version(info.ptr, self.version))?;
            check(sys::ffpsd_version_info_set_has_real_merged_data(
                info.ptr,
                self.has_real_merged_data.into(),
            ))?;
            check(sys::ffpsd_version_info_set_writer_name(info.ptr, writer_name.as_ptr()))?;
            check(sys::ffpsd_version_info_set_reader_name(info.ptr, reader_name.as_ptr()))?;
            check(sys::ffpsd_version_info_set_file_version(info.ptr, self.file_version))?;
        }
        Ok(info)
    }
}

/// One record of a Levels adjustment.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LevelsChannel {
    pub input_floor: u16,
    pub input_ceiling: u16,
    pub output_floor: u16,
    pub output_ceiling: u16,
    pub gamma: f64,
}

impl LevelsChannel {
    /// A record that changes nothing.
    pub const IDENTITY: Self = Self {
        input_floor: 0,
        input_ceiling: 255,
        output_floor: 0,
        output_ceiling: 255,
        gamma: 1.0,
    };
}

impl Default for LevelsChannel {
    fn default() -> Self {
        Self::IDENTITY
    }
}

/// A Levels adjustment: record 0 is every color channel at once, record 1 channel 0 and so on.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct Levels {
    pub channels: Vec<LevelsChannel>,
}

impl Levels {
    pub(crate) fn take(levels: *mut sys::ffpsd_levels_t) -> Result<Self> {
        let levels = Owned::new(levels, sys::ffpsd_levels_destroy);
        let count = unsafe { sys::ffpsd_levels_get_count(levels.ptr) };
        let mut channels = Vec::with_capacity(count);
        for index in 0..count {
            let mut channel = sys::ffpsd_levels_channel_t {
                input_floor: 0,
                input_ceiling: 0,
                output_floor: 0,
                output_ceiling: 0,
                gamma: 0.0,
            };
            check(unsafe { sys::ffpsd_levels_get_channel(levels.ptr, index, &mut channel) })?;
            channels.push(LevelsChannel {
                input_floor: channel.input_floor,
                input_ceiling: channel.input_ceiling,
                output_floor: channel.output_floor,
                output_ceiling: channel.output_ceiling,
                gamma: channel.gamma,
            });
        }
        Ok(Self { channels })
    }

    pub(crate) fn to_raw(&self) -> Result<Owned<sys::ffpsd_levels_t>> {
        let mut levels = ptr::null_mut();
        check(unsafe { sys::ffpsd_levels_create(&mut levels) })?;
        let levels = Owned::new(levels, sys::ffpsd_levels_destroy);
        for (index, channel) in self.channels.iter().enumerate() {
            let raw = sys::ffpsd_levels_channel_t {
                input_floor: channel.input_floor,
                input_ceiling: channel.input_ceiling,
                output_floor: channel.output_floor,
                output_ceiling: channel.output_ceiling,
                gamma: channel.gamma,
            };
            check(unsafe { sys::ffpsd_levels_set_channel(levels.ptr, index, &raw) })?;
        }
        Ok(levels)
    }
}

/// An image resource of section 3, copied out of the document.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImageResource {
    pub id: u16,
    pub name: String,
    pub data: Vec<u8>,
}

impl ImageResource {
    pub(crate) fn from_raw(raw: &sys::ffpsd_image_resource_t) -> Self {
        Self {
            id: raw.id,
            name: unsafe { string(raw.name) },
            data: unsafe { bytes(raw.data, raw.size) }.to_vec(),
        }
    }
}

/// A tagged block, copied out of the document or layer; key and signature as fourcc makes them.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TaggedBlock {
    pub signature: u32,
    pub key: u32,
    pub data: Vec<u8>,
}

impl TaggedBlock {
    pub(crate) fn from_raw(raw: &sys::ffpsd_tagged_block_t) -> Self {
        Self {
            signature: raw.signature,
            key: raw.key,
            data: unsafe { bytes(raw.data, raw.size) }.to_vec(),
        }
    }

    pub(crate) fn to_raw(&self) -> sys::ffpsd_tagged_block_t {
        sys::ffpsd_tagged_block_t {
            signature: self.signature,
            key: self.key,
            data: self.data.as_ptr(),
            size: self.data.len(),
        }
    }
}

/// A layer channel as the file stores it; size counts the compression field and RLE row counts.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ChannelInfo {
    pub id: i16,
    pub compression: ChannelCompression,
    pub size: u64,
}

/// A layer's pixel mask.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Mask {
    pub image: Image,
    pub bounds: Rect,
    /// The value outside the bounds.
    pub default_color: u8,
}

/// Something ffpsd made, destroyed with it.
pub(crate) struct Owned<T> {
    pub(crate) ptr: *mut T,
    destroy: unsafe extern "C" fn(*mut T),
}

impl<T> Owned<T> {
    pub(crate) fn new(ptr: *mut T, destroy: unsafe extern "C" fn(*mut T)) -> Self {
        Self { ptr, destroy }
    }
}

impl<T> Drop for Owned<T> {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { (self.destroy)(self.ptr) };
        }
    }
}

/// Copies a buffer ffpsd made and destroys it.
pub(crate) fn take_buffer(buffer: *mut sys::ffpsd_buffer_t) -> Vec<u8> {
    let buffer = Owned::new(buffer, sys::ffpsd_buffer_destroy);
    unsafe { bytes(sys::ffpsd_buffer_get_data(buffer.ptr), sys::ffpsd_buffer_get_size(buffer.ptr)) }.to_vec()
}

pub(crate) fn empty_view() -> sys::ffpsd_image_view_t {
    sys::ffpsd_image_view_t {
        width: 0,
        height: 0,
        channel_count: 0,
        depth: 0,
        color_mode: sys::FFPSD_COLOR_MODE_RGB,
        data: ptr::null(),
        size: 0,
    }
}

/// A slice over memory ffpsd lent; empty for a null pointer, which slice::from_raw_parts does not allow.
pub(crate) unsafe fn bytes<'a>(data: *const u8, size: usize) -> &'a [u8] {
    if data.is_null() || size == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(data, size) }
    }
}

/// An owned copy of a string ffpsd lent; empty for a null pointer.
pub(crate) unsafe fn string(text: *const c_char) -> String {
    if text.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(text) }.to_string_lossy().into_owned()
    }
}
