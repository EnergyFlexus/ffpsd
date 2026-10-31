# ffpsd

Rust bindings to [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes Photoshop PSD
and PSB files.

```rust
use ffpsd::formats::{load_picture, save_png_to_memory};
use ffpsd::{fourcc, Levels, LevelsChannel};
use ffpsd::{Anchor, Document, Result};
use ffpsd::{ColorMode, ImageInfo, ImageView};
use ffpsd::{Compression, PngCompression};
use ffpsd::{FlipDirection, ResampleFilter};

fn run(input: &str, picture: &str, output: &str) -> Result<()> {
    let mut doc = Document::open(input)?;
    let (width, height) = (doc.width(), doc.height());
    let (depth, count) = (doc.depth(), doc.layer_count());
    println!("{width} x {height}, {depth} bit, {count} layers");

    // The composite Photoshop saved with the file, as PNG.
    let composite = doc.merged_image()?;
    let compression = PngCompression::Balanced;
    let png = save_png_to_memory(composite.view(), compression)?;
    println!("composite: {} bytes of PNG", png.len());

    // The top layer is the last; with no layers, layer() fails.
    let top_index = count.wrapping_sub(1);
    let top = doc.layer(top_index)?;
    let (name, rect) = (top.name(), top.bounds());
    let (w, h) = (rect.width(), rect.height());
    println!("top layer: {name}, {w} x {h}");

    // A copy of it on top, half transparent, in Multiply mode.
    let mut copy = doc.duplicate_layer(top_index)?;
    copy.set_name("Copy")?;
    copy.set_opacity(128)?;
    copy.set_blend_key(fourcc(b"mul "))?;

    // A PNG, JPEG or WebP at (50, 50), half the canvas, mirrored.
    let image = load_picture(picture, doc.color_mode(), depth)?;
    let view = Some(image.view());
    let mut layer = doc.add_layer("Picture", view, 50, 50)?;
    layer.resize(width / 2, height / 2, ResampleFilter::Bicubic)?;
    layer.flip(FlipDirection::Horizontal)?;

    // A mask that hides a 100 x 100 square in its corner.
    let hidden = vec![0u8; 100 * 100];
    let info = ImageInfo {
        width: 100,
        height: 100,
        channel_count: 1,
        depth: 8,
        color_mode: ColorMode::Grayscale,
    };
    layer.set_mask(ImageView::new(info, &hidden), 50, 50, 255)?;

    // A Levels layer over everything that brightens the midtones.
    let brighter = LevelsChannel {
        gamma: 1.5,
        ..LevelsChannel::IDENTITY
    };
    let levels = Levels { channels: vec![brighter] };
    doc.add_levels_layer("Levels", Some(&levels))?;

    // 100 px more canvas on every side.
    doc.resize_canvas(width + 200, height + 200, Anchor::Center)?;
    // Each channel RLE or raw, whichever is smaller.
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
