# ffpsd

Rust bindings to [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes Photoshop PSD
and PSB files.

```rust
use ffpsd::formats::{load_picture, save_png_to_memory};
use ffpsd::{
    fourcc, Anchor, ColorMode, Compression, Document, FlipDirection, ImageInfo, ImageView, Levels, LevelsChannel, PngCompression,
    ResampleFilter,
};

fn run(input: &str, picture_path: &str, output: &str) -> ffpsd::Result<()> {
    let mut doc = Document::open(input)?;
    let (width, height) = (doc.width(), doc.height());
    println!("{width} x {height}, {} bit, {} layers", doc.depth(), doc.layer_count());

    // The composite Photoshop saved with the file, as PNG.
    let png = save_png_to_memory(doc.merged_image()?.view(), PngCompression::Balanced)?;
    println!("composite: {} bytes of PNG", png.len());

    // Layers go from the bottom up, so the top one is the last; with none, layer() fails.
    let top_index = doc.layer_count().wrapping_sub(1);
    let top = doc.layer(top_index)?;
    let bounds = top.bounds();
    println!("top layer: {}, {} x {}", top.name(), bounds.width(), bounds.height());

    // A copy of it on top, half transparent, in Multiply mode.
    let mut copy = doc.duplicate_layer(top_index)?;
    copy.set_name("Copy")?;
    copy.set_opacity(128)?;
    copy.set_blend_key(fourcc(b"mul "))?;

    // A PNG, JPEG or WebP as a layer in the document's mode and depth, 50 px from the corner, half the canvas in size, mirrored.
    let image = load_picture(picture_path, doc.color_mode(), doc.depth())?;
    let mut picture = doc.add_layer("Picture", Some(image.view()), 50, 50)?;
    picture.resize(width / 2, height / 2, ResampleFilter::Bicubic)?;
    picture.flip(FlipDirection::Horizontal)?;

    // A mask that hides a 100 x 100 square in the picture's corner and shows the rest.
    let hidden = vec![0u8; 100 * 100];
    let info = ImageInfo {
        width: 100,
        height: 100,
        channel_count: 1,
        depth: 8,
        color_mode: ColorMode::Grayscale,
    };
    picture.set_mask(ImageView::new(info, &hidden), 50, 50, 255)?;

    // A Levels layer over everything that brightens the midtones.
    let brighter = LevelsChannel {
        gamma: 1.5,
        ..LevelsChannel::IDENTITY
    };
    doc.add_levels_layer("Levels", Some(&Levels { channels: vec![brighter] }))?;

    // 100 px more canvas on every side, then save, each channel RLE or raw, whichever is smaller.
    doc.resize_canvas(width + 200, height + 200, Anchor::Center)?;
    doc.save(output, Compression::RleOrRaw)?;
    println!("saved {} layers to {output}", doc.layer_count());
    Ok(())
}
```

## What it does for now

* PSD and PSB, and either into the other; RGB and gray documents, and either into the other.
* Layers: raster layers, groups, the background and Levels adjustments; added, copied from this or another
  document, moved and removed; name, opacity, visibility, clipping and blend mode; pixels in and out.
* Pixel masks, read, set and removed.
* The canvas and the layers resized, turned and flipped; the canvas grown or cut from any anchor.
* The composite the file stores, read and replaced; resolution and version info.
* Every image resource and tagged block, raw, for what ffpsd does not interpret itself.
* PNG, JPEG and WebP in and out.

## In Rust

* A layer borrows its document: `Layer` to read, `LayerMut` to change, so a removed layer cannot be used and
  the borrow checker keeps one writer at a time.
* `Document` is `Send` but not `Sync`: it moves between threads, and separate documents work in threads at once.
* Every call that can fail returns `ffpsd::Result`; `Error` has a `kind()` and a readable message.
* Pixels are `Image` and `ImageView`: planar as PSD keeps them, 16 and 32 bit samples in native byte order.
* The library comes from [ffpsd-sys](https://crates.io/crates/ffpsd-sys): the static library of the GitHub
  release, checked by SHA-256, for x86_64 Windows (MSVC), x86_64 Linux and Apple Silicon macOS; elsewhere,
  or with `FFPSD_FROM_SOURCE=1`, a build from source with CMake 3.23 or later and a C++17 compiler.

More in the [repository](https://github.com/EnergyFlexus/ffpsd): building from source and the
[roadmap](https://github.com/EnergyFlexus/ffpsd/blob/main/ROADMAP.md).

## License

MIT. The vendored libraries built in keep their own permissive licenses, which ffpsd-sys carries.
