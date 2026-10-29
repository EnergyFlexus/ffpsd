use crate::document::Document;
use crate::error::{c_string, check, check_found, Result};
use crate::types::{
    empty_view, ChannelCompression, ChannelInfo, FlipDirection, Image, ImageInfo, ImageView, LayerKind, Levels, Mask,
    Rect, ResampleFilter, Rotation, TaggedBlock,
};
use ffpsd_sys as sys;
use std::fmt;
use std::marker::PhantomData;
use std::ops::Deref;
use std::ptr;

/// A layer read through a shared borrow of its document.
#[derive(Clone, Copy)]
pub struct Layer<'a> {
    pub(crate) ptr: *mut sys::ffpsd_layer_t,
    _document: PhantomData<&'a Document>,
}

/// A layer changed through a mutable borrow of its document; it reads as a Layer too.
pub struct LayerMut<'a> {
    layer: Layer<'a>,
    _document: PhantomData<&'a mut Document>,
}

/// A failure here means a null handle, which a Layer never holds.
fn expect_ok(status: sys::ffpsd_status_t) {
    if let Err(error) = check(status) {
        panic!("ffpsd failed on a valid layer: {error}");
    }
}

impl<'a> Layer<'a> {
    pub(crate) fn new(ptr: *mut sys::ffpsd_layer_t) -> Self {
        Self {
            ptr,
            _document: PhantomData,
        }
    }

    pub fn kind(&self) -> LayerKind {
        LayerKind::from_raw(unsafe { sys::ffpsd_layer_get_kind(self.ptr) }).expect("ffpsd returned a layer kind")
    }

    pub fn bounds(&self) -> Rect {
        let mut bounds = sys::ffpsd_rect_t {
            top: 0,
            left: 0,
            bottom: 0,
            right: 0,
        };
        expect_ok(unsafe { sys::ffpsd_layer_get_bounds(self.ptr, &mut bounds) });
        Rect::from_raw(bounds)
    }

    pub fn name(&self) -> String {
        let mut length = 0usize;
        expect_ok(unsafe { sys::ffpsd_layer_get_name(self.ptr, ptr::null_mut(), 0, &mut length) });
        let mut buffer = vec![0u8; length + 1];
        expect_ok(unsafe {
            sys::ffpsd_layer_get_name(self.ptr, buffer.as_mut_ptr().cast(), buffer.len(), &mut length)
        });
        buffer.truncate(length);
        String::from_utf8_lossy(&buffer).into_owned()
    }

    pub fn opacity(&self) -> u8 {
        unsafe { sys::ffpsd_layer_get_opacity(self.ptr) }
    }

    pub fn is_visible(&self) -> bool {
        unsafe { sys::ffpsd_layer_is_visible(self.ptr) != 0 }
    }

    pub fn is_clipped(&self) -> bool {
        unsafe { sys::ffpsd_layer_is_clipped(self.ptr) != 0 }
    }

    pub fn is_background(&self) -> bool {
        unsafe { sys::ffpsd_layer_is_background(self.ptr) != 0 }
    }

    /// Such as `fourcc(b"norm")`.
    pub fn blend_key(&self) -> u32 {
        unsafe { sys::ffpsd_layer_get_blend_key(self.ptr) }
    }

    /// The block of an adjustment layer, such as `fourcc(b"levl")`.
    pub fn adjustment_key(&self) -> Option<u32> {
        match unsafe { sys::ffpsd_layer_get_adjustment_key(self.ptr) } {
            0 => None,
            key => Some(key),
        }
    }

    /// None unless this is a Levels layer.
    pub fn levels(&self) -> Result<Option<Levels>> {
        let mut levels = ptr::null_mut();
        if !check_found(unsafe { sys::ffpsd_layer_get_levels(self.ptr, &mut levels) })? {
            return Ok(None);
        }
        Levels::take(levels).map(Some)
    }

    /// Color planes by channel id, then transparency when the layer has one.
    pub fn pixels(&self) -> Result<Image> {
        Image::read(
            |view| unsafe { sys::ffpsd_layer_get_pixels_info(self.ptr, view) },
            |data, size| unsafe { sys::ffpsd_layer_get_pixels_bytes(self.ptr, data, size) },
        )
    }

    /// The shape of pixels() without decoding them.
    pub fn pixels_info(&self) -> Result<ImageInfo> {
        let mut view = empty_view();
        check(unsafe { sys::ffpsd_layer_get_pixels_info(self.ptr, &mut view) })?;
        ImageInfo::from_raw(&view)
    }

    /// None without a pixel mask; Photoshop's rendering of a vector mask is not one.
    pub fn mask(&self) -> Result<Option<Mask>> {
        let mut image = ptr::null_mut();
        let mut bounds = sys::ffpsd_rect_t {
            top: 0,
            left: 0,
            bottom: 0,
            right: 0,
        };
        let mut default_color = 0u8;
        if !check_found(unsafe { sys::ffpsd_layer_get_mask(self.ptr, &mut image, &mut bounds, &mut default_color) })? {
            return Ok(None);
        }
        Ok(Some(Mask {
            image: Image::take(image)?,
            bounds: Rect::from_raw(bounds),
            default_color,
        }))
    }

    /// In file order, masks included.
    pub fn channels(&self) -> Vec<ChannelInfo> {
        let count = unsafe { sys::ffpsd_layer_get_channel_count(self.ptr) };
        (0..count)
            .map(|index| {
                let mut info = sys::ffpsd_channel_info_t {
                    id: 0,
                    compression: 0,
                    size: 0,
                };
                expect_ok(unsafe { sys::ffpsd_layer_get_channel_by_index(self.ptr, index, &mut info) });
                ChannelInfo {
                    id: info.id,
                    compression: ChannelCompression::from_raw(info.compression).expect("ffpsd returned a compression"),
                    size: info.size,
                }
            })
            .collect()
    }

    pub fn tagged_block_count(&self) -> usize {
        unsafe { sys::ffpsd_layer_get_tagged_block_count(self.ptr) }
    }

    pub fn tagged_block(&self, index: usize) -> Result<TaggedBlock> {
        let mut block = empty_block();
        check(unsafe { sys::ffpsd_layer_get_tagged_block_by_index(self.ptr, index, &mut block) })?;
        Ok(TaggedBlock::from_raw(&block))
    }

    /// Keys may repeat; this is the first.
    pub fn find_tagged_block(&self, key: u32) -> Result<Option<TaggedBlock>> {
        let mut block = empty_block();
        let found = check_found(unsafe { sys::ffpsd_layer_get_tagged_block_by_key(self.ptr, key, &mut block) })?;
        Ok(found.then(|| TaggedBlock::from_raw(&block)))
    }
}

impl fmt::Debug for Layer<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Layer")
            .field("name", &self.name())
            .field("kind", &self.kind())
            .field("bounds", &self.bounds())
            .finish()
    }
}

impl<'a> LayerMut<'a> {
    pub(crate) fn new(ptr: *mut sys::ffpsd_layer_t) -> Self {
        Self {
            layer: Layer::new(ptr),
            _document: PhantomData,
        }
    }

    pub fn set_name(&mut self, name: &str) -> Result<()> {
        let name = c_string(name)?;
        check(unsafe { sys::ffpsd_layer_set_name(self.ptr, name.as_ptr()) })
    }

    pub fn set_opacity(&mut self, opacity: u8) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_opacity(self.ptr, opacity) })
    }

    pub fn set_visible(&mut self, visible: bool) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_visible(self.ptr, visible.into()) })
    }

    pub fn set_clipped(&mut self, clipped: bool) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_clipped(self.ptr, clipped.into()) })
    }

    /// A group's header only: whether Photoshop's panel shows its layers.
    pub fn set_group_open(&mut self, open: bool) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_group_open(self.ptr, open.into()) })
    }

    pub fn set_blend_key(&mut self, blend_key: u32) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_blend_key(self.ptr, blend_key) })
    }

    pub fn set_levels(&mut self, levels: &Levels) -> Result<()> {
        let levels = levels.to_raw()?;
        check(unsafe { sys::ffpsd_layer_set_levels(self.ptr, levels.ptr) })
    }

    /// A raster layer without a vector mask; the background does not move or resize.
    pub fn set_position(&mut self, top: i32, left: i32) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_set_position(self.ptr, top, left) })
    }

    pub fn resize(&mut self, width: u32, height: u32, filter: ResampleFilter) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_resize(self.ptr, width, height, filter.to_raw()) })
    }

    pub fn flip(&mut self, direction: FlipDirection) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_flip(self.ptr, direction.to_raw()) })
    }

    pub fn rotate(&mut self, rotation: Rotation) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_rotate(self.ptr, rotation.to_raw()) })
    }

    pub fn set_pixels(&mut self, image: ImageView<'_>) -> Result<()> {
        let image = image.to_raw();
        check(unsafe { sys::ffpsd_layer_set_pixels(self.ptr, &image) })
    }

    /// The background and a layer with a vector mask take none.
    pub fn set_mask(&mut self, image: ImageView<'_>, top: i32, left: i32, default_color: u8) -> Result<()> {
        let image = image.to_raw();
        check(unsafe { sys::ffpsd_layer_set_mask(self.ptr, &image, top, left, default_color) })
    }

    pub fn remove_mask(&mut self) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_remove_mask(self.ptr) })
    }

    pub fn set_tagged_block(&mut self, block: &TaggedBlock) -> Result<()> {
        let block = block.to_raw();
        check(unsafe { sys::ffpsd_layer_set_tagged_block(self.ptr, &block) })
    }

    /// Keys may repeat; this removes the first.
    pub fn remove_tagged_block(&mut self, key: u32) -> Result<()> {
        check(unsafe { sys::ffpsd_layer_remove_tagged_block(self.ptr, key) })
    }
}

impl<'a> Deref for LayerMut<'a> {
    type Target = Layer<'a>;

    fn deref(&self) -> &Layer<'a> {
        &self.layer
    }
}

impl fmt::Debug for LayerMut<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.layer.fmt(f)
    }
}

pub(crate) fn empty_block() -> sys::ffpsd_tagged_block_t {
    sys::ffpsd_tagged_block_t {
        signature: 0,
        key: 0,
        data: ptr::null(),
        size: 0,
    }
}
