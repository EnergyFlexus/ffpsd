use crate::error::{c_path, c_string, check, check_found, Result};
use crate::layer::{empty_block, Layer, LayerMut};
use crate::types::{
    empty_view, take_buffer, Anchor, ChannelCompression, ColorMode, Compression, FlipDirection, Image, ImageInfo, ImageResource, ImageView,
    Levels, ResampleFilter, ResolutionInfo, Rotation, TaggedBlock, VersionInfo,
};
use ffpsd_sys as sys;
use std::fmt;
use std::iter::FusedIterator;
use std::path::Path;
use std::ptr;

/// A PSD or PSB file in memory, with its layers.
pub struct Document {
    ptr: *mut sys::ffpsd_document_t,
}

// ffpsd keeps no state outside its objects, so a document may move to another thread; it is not Sync,
// as reads of a layer may fill caches inside it.
unsafe impl Send for Document {}

impl Drop for Document {
    fn drop(&mut self) {
        unsafe { sys::ffpsd_document_destroy(self.ptr) };
    }
}

impl Document {
    fn make(create: impl FnOnce(&mut *mut sys::ffpsd_document_t) -> sys::ffpsd_status_t) -> Result<Self> {
        let mut ptr = ptr::null_mut();
        check(create(&mut ptr))?;
        Ok(Self { ptr })
    }

    /// An empty canvas; channels follow the mode, and a multichannel document is refused.
    pub fn new(width: u32, height: u32, color_mode: ColorMode, depth: u16) -> Result<Self> {
        Self::make(|out| unsafe { sys::ffpsd_document_create_with(width, height, color_mode.to_raw(), depth, out) })
    }

    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let path = c_path(path.as_ref())?;
        Self::make(|out| unsafe { sys::ffpsd_document_open(path.as_ptr(), out) })
    }

    pub fn open_from_memory(data: &[u8]) -> Result<Self> {
        Self::make(|out| unsafe { sys::ffpsd_document_open_memory(data.as_ptr(), data.len(), out) })
    }

    pub fn save(&self, path: impl AsRef<Path>, compression: Compression) -> Result<()> {
        let path = c_path(path.as_ref())?;
        check(unsafe { sys::ffpsd_document_save(self.ptr, path.as_ptr(), compression.to_raw()) })
    }

    pub fn save_to_memory(&self, compression: Compression) -> Result<Vec<u8>> {
        let mut buffer = ptr::null_mut();
        check(unsafe { sys::ffpsd_document_save_memory(self.ptr, compression.to_raw(), &mut buffer) })?;
        Ok(take_buffer(buffer))
    }

    pub fn width(&self) -> u32 {
        unsafe { sys::ffpsd_document_get_width(self.ptr) }
    }

    pub fn height(&self) -> u32 {
        unsafe { sys::ffpsd_document_get_height(self.ptr) }
    }

    pub fn channel_count(&self) -> u16 {
        unsafe { sys::ffpsd_document_get_channel_count(self.ptr) }
    }

    pub fn depth(&self) -> u16 {
        unsafe { sys::ffpsd_document_get_depth(self.ptr) }
    }

    pub fn color_mode(&self) -> ColorMode {
        ColorMode::from_raw(unsafe { sys::ffpsd_document_get_color_mode(self.ptr) }).expect("ffpsd returned a color mode")
    }

    pub fn is_psb(&self) -> bool {
        unsafe { sys::ffpsd_document_is_psb(self.ptr) != 0 }
    }

    pub fn has_real_merged_data(&self) -> bool {
        unsafe { sys::ffpsd_document_get_has_real_merged_data(self.ptr) != 0 }
    }

    /// Text, smart objects, shapes and vector masks are refused.
    pub fn resize_canvas(&mut self, width: u32, height: u32, anchor: Anchor) -> Result<()> {
        check(unsafe { sys::ffpsd_document_resize_canvas(self.ptr, width, height, anchor.to_raw()) })
    }

    /// Text, smart objects and shapes are refused.
    pub fn resize(&mut self, width: u32, height: u32, filter: ResampleFilter) -> Result<()> {
        check(unsafe { sys::ffpsd_document_resize(self.ptr, width, height, filter.to_raw()) })
    }

    pub fn flip_canvas(&mut self, direction: FlipDirection) -> Result<()> {
        check(unsafe { sys::ffpsd_document_flip_canvas(self.ptr, direction.to_raw()) })
    }

    pub fn rotate_canvas(&mut self, rotation: Rotation) -> Result<()> {
        check(unsafe { sys::ffpsd_document_rotate_canvas(self.ptr, rotation.to_raw()) })
    }

    pub fn set_psb(&mut self, psb: bool) -> Result<()> {
        check(unsafe { sys::ffpsd_document_set_psb(self.ptr, psb.into()) })
    }

    /// RGB to grayscale and back; any other pair is refused.
    pub fn convert_color_mode(&mut self, color_mode: ColorMode) -> Result<()> {
        check(unsafe { sys::ffpsd_document_convert_color_mode(self.ptr, color_mode.to_raw()) })
    }

    pub fn set_has_real_merged_data(&mut self, value: bool) -> Result<()> {
        check(unsafe { sys::ffpsd_document_set_has_real_merged_data(self.ptr, value.into()) })
    }

    /// 72 dpi when the file has no resource 1005.
    pub fn resolution_info(&self) -> Result<ResolutionInfo> {
        let mut info = sys::ffpsd_resolution_info_t {
            horizontal: 0.0,
            horizontal_unit: 0,
            width_unit: 0,
            vertical: 0.0,
            vertical_unit: 0,
            height_unit: 0,
        };
        check(unsafe { sys::ffpsd_document_get_resolution_info(self.ptr, &mut info) })?;
        Ok(ResolutionInfo::from_raw(&info))
    }

    pub fn set_resolution_info(&mut self, info: &ResolutionInfo) -> Result<()> {
        let info = info.to_raw();
        check(unsafe { sys::ffpsd_document_set_resolution_info(self.ptr, &info) })
    }

    pub fn version_info(&self) -> Result<VersionInfo> {
        let mut info = ptr::null_mut();
        check(unsafe { sys::ffpsd_document_get_version_info(self.ptr, &mut info) })?;
        Ok(VersionInfo::take(info))
    }

    pub fn set_version_info(&mut self, info: &VersionInfo) -> Result<()> {
        let info = info.to_raw()?;
        check(unsafe { sys::ffpsd_document_set_version_info(self.ptr, info.ptr) })
    }

    pub fn image_resource_count(&self) -> usize {
        unsafe { sys::ffpsd_document_get_image_resource_count(self.ptr) }
    }

    pub fn image_resource(&self, index: usize) -> Result<ImageResource> {
        let mut resource = empty_resource();
        check(unsafe { sys::ffpsd_document_get_image_resource_by_index(self.ptr, index, &mut resource) })?;
        Ok(ImageResource::from_raw(&resource))
    }

    pub fn find_image_resource(&self, id: u16) -> Result<Option<ImageResource>> {
        let mut resource = empty_resource();
        let found = check_found(unsafe { sys::ffpsd_document_get_image_resource_by_id(self.ptr, id, &mut resource) })?;
        Ok(found.then(|| ImageResource::from_raw(&resource)))
    }

    /// Replaces the resource of that id, or inserts it in id order.
    pub fn set_image_resource(&mut self, resource: &ImageResource) -> Result<()> {
        let name = c_string(&resource.name)?;
        let raw = sys::ffpsd_image_resource_t {
            id: resource.id,
            name: name.as_ptr(),
            data: resource.data.as_ptr(),
            size: resource.data.len(),
        };
        check(unsafe { sys::ffpsd_document_set_image_resource(self.ptr, &raw) })
    }

    pub fn remove_image_resource(&mut self, id: u16) -> Result<()> {
        check(unsafe { sys::ffpsd_document_remove_image_resource(self.ptr, id) })
    }

    pub fn layer_count(&self) -> usize {
        unsafe { sys::ffpsd_document_get_layer_count(self.ptr) }
    }

    fn layer_ptr(&self, index: usize) -> Result<*mut sys::ffpsd_layer_t> {
        let mut layer = ptr::null_mut();
        check(unsafe { sys::ffpsd_document_get_layer(self.ptr, index, &mut layer) })?;
        Ok(layer)
    }

    /// Bottom to top.
    pub fn layer(&self, index: usize) -> Result<Layer<'_>> {
        self.layer_ptr(index).map(Layer::new)
    }

    pub fn layer_mut(&mut self, index: usize) -> Result<LayerMut<'_>> {
        self.layer_ptr(index).map(LayerMut::new)
    }

    /// Bottom to top.
    pub fn layers(&self) -> Layers<'_> {
        Layers {
            document: self,
            next: 0,
            end: self.layer_count(),
        }
    }

    /// Where the layer is now; None for a layer of another document.
    pub fn find_layer_index(&self, layer: &Layer<'_>) -> Option<usize> {
        let mut index = 0usize;
        let found = check_found(unsafe { sys::ffpsd_document_find_layer_index(self.ptr, layer.ptr, &mut index) });
        matches!(found, Ok(true)).then_some(index)
    }

    fn add(&mut self, add: impl FnOnce(&mut *mut sys::ffpsd_layer_t) -> sys::ffpsd_status_t) -> Result<LayerMut<'_>> {
        let mut layer = ptr::null_mut();
        check(add(&mut layer))?;
        Ok(LayerMut::new(layer))
    }

    /// A raster layer on top; no image makes an empty layer.
    pub fn add_layer(&mut self, name: &str, image: Option<ImageView<'_>>, top: i32, left: i32) -> Result<LayerMut<'_>> {
        let name = c_string(name)?;
        let image = image.map(ImageView::to_raw);
        let doc = self.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_layer(doc, name.as_ptr(), optional(&image), top, left, out) })
    }

    /// Photoshop's locked background: at the bottom, the document's size, one at most.
    pub fn add_background_layer(&mut self, name: &str, image: Option<ImageView<'_>>) -> Result<LayerMut<'_>> {
        let name = c_string(name)?;
        let image = image.map(ImageView::to_raw);
        let doc = self.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_background_layer(doc, name.as_ptr(), optional(&image), out) })
    }

    /// A raster layer without a mask becomes the background, over white and fitted to the canvas.
    pub fn set_background_layer(&mut self, index: usize) -> Result<()> {
        check(unsafe { sys::ffpsd_document_set_background_layer(self.ptr, index) })
    }

    /// The background becomes an ordinary layer where it is; false when there is none.
    pub fn unset_background_layer(&mut self) -> Result<bool> {
        check_found(unsafe { sys::ffpsd_document_unset_background_layer(self.ptr) })
    }

    /// An empty group on top, returned by its header; a layer moved between its end marker and its header is inside.
    pub fn add_group(&mut self, name: &str) -> Result<LayerMut<'_>> {
        let name = c_string(name)?;
        let doc = self.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_group(doc, name.as_ptr(), out) })
    }

    /// No levels make one that changes nothing.
    pub fn add_levels_layer(&mut self, name: &str, levels: Option<&Levels>) -> Result<LayerMut<'_>> {
        let name = c_string(name)?;
        let levels = levels.map(Levels::to_raw).transpose()?;
        let levels = levels.as_ref().map_or(ptr::null(), |levels| levels.ptr.cast_const());
        let doc = self.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_levels_layer(doc, name.as_ptr(), levels, out) })
    }

    /// A copy on top of a layer of a document with the same depth, color mode and format; a group's header brings the
    /// whole group. For a layer of this document, see duplicate_layer.
    pub fn add_layer_copy(&mut self, source: &Layer<'_>) -> Result<LayerMut<'_>> {
        let doc = self.ptr;
        let source = source.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_layer_copy(doc, source, out) })
    }

    /// A copy of one of this document's layers on top.
    pub fn duplicate_layer(&mut self, index: usize) -> Result<LayerMut<'_>> {
        let source = self.layer_ptr(index)?;
        let doc = self.ptr;
        self.add(|out| unsafe { sys::ffpsd_document_add_layer_copy(doc, source, out) })
    }

    pub fn remove_layer(&mut self, index: usize) -> Result<()> {
        check(unsafe { sys::ffpsd_document_remove_layer(self.ptr, index) })
    }

    pub fn move_layer(&mut self, from: usize, to: usize) -> Result<()> {
        check(unsafe { sys::ffpsd_document_move_layer(self.ptr, from, to) })
    }

    /// The blocks of section 4 after the layers.
    pub fn tagged_block_count(&self) -> usize {
        unsafe { sys::ffpsd_document_get_tagged_block_count(self.ptr) }
    }

    pub fn tagged_block(&self, index: usize) -> Result<TaggedBlock> {
        let mut block = empty_block();
        check(unsafe { sys::ffpsd_document_get_tagged_block_by_index(self.ptr, index, &mut block) })?;
        Ok(TaggedBlock::from_raw(&block))
    }

    pub fn find_tagged_block(&self, key: u32) -> Result<Option<TaggedBlock>> {
        let mut block = empty_block();
        let found = check_found(unsafe { sys::ffpsd_document_get_tagged_block_by_key(self.ptr, key, &mut block) })?;
        Ok(found.then(|| TaggedBlock::from_raw(&block)))
    }

    pub fn set_tagged_block(&mut self, block: &TaggedBlock) -> Result<()> {
        let block = block.to_raw();
        check(unsafe { sys::ffpsd_document_set_tagged_block(self.ptr, &block) })
    }

    pub fn remove_tagged_block(&mut self, key: u32) -> Result<()> {
        check(unsafe { sys::ffpsd_document_remove_tagged_block(self.ptr, key) })
    }

    /// The composite; an empty image when the file has none.
    pub fn merged_image(&self) -> Result<Image> {
        Image::read(
            |view| unsafe { sys::ffpsd_document_get_merged_image_info(self.ptr, view) },
            |data, size| unsafe { sys::ffpsd_document_get_merged_image_bytes(self.ptr, data, size) },
        )
    }

    /// The shape of merged_image() without decoding it.
    pub fn merged_image_info(&self) -> Result<ImageInfo> {
        let mut view = empty_view();
        check(unsafe { sys::ffpsd_document_get_merged_image_info(self.ptr, &mut view) })?;
        ImageInfo::from_raw(&view)
    }

    /// Also sets has_real_merged_data.
    pub fn set_merged_image(&mut self, image: ImageView<'_>) -> Result<()> {
        let image = image.to_raw();
        check(unsafe { sys::ffpsd_document_set_merged_image(self.ptr, &image) })
    }

    /// None when the file has no composite.
    pub fn merged_compression(&self) -> Result<Option<ChannelCompression>> {
        let mut compression = 0;
        if !check_found(unsafe { sys::ffpsd_document_get_merged_compression(self.ptr, &mut compression) })? {
            return Ok(None);
        }
        ChannelCompression::from_raw(compression).map(Some)
    }
}

impl fmt::Debug for Document {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Document")
            .field("width", &self.width())
            .field("height", &self.height())
            .field("color_mode", &self.color_mode())
            .field("depth", &self.depth())
            .field("layer_count", &self.layer_count())
            .finish()
    }
}

/// The layers of a document, bottom to top.
pub struct Layers<'a> {
    document: &'a Document,
    next: usize,
    end: usize,
}

impl<'a> Iterator for Layers<'a> {
    type Item = Layer<'a>;

    fn next(&mut self) -> Option<Layer<'a>> {
        if self.next == self.end {
            return None;
        }
        self.next += 1;
        Some(self.document.layer(self.next - 1).expect("an index below the count"))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.end - self.next, Some(self.end - self.next))
    }
}

impl DoubleEndedIterator for Layers<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.next == self.end {
            return None;
        }
        self.end -= 1;
        Some(self.document.layer(self.end).expect("an index below the count"))
    }
}

impl ExactSizeIterator for Layers<'_> {}
impl FusedIterator for Layers<'_> {}

fn empty_resource() -> sys::ffpsd_image_resource_t {
    sys::ffpsd_image_resource_t {
        id: 0,
        name: ptr::null(),
        data: ptr::null(),
        size: 0,
    }
}

/// A pointer to the view, or null for none.
fn optional(image: &Option<sys::ffpsd_image_view_t>) -> *const sys::ffpsd_image_view_t {
    image.as_ref().map_or(ptr::null(), |image| image as *const _)
}
