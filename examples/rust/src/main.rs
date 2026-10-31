//! Opens a PSD, adds a copy of the top layer, a picture with a mask and a Levels layer, grows the canvas and saves the result.
use ffpsd::formats::{load_picture, save_png_to_memory};
use ffpsd::{fourcc, Levels, LevelsChannel};
use ffpsd::{Anchor, Document, Result};
use ffpsd::{ColorMode, ImageInfo, ImageView};
use ffpsd::{Compression, PngCompression};
use ffpsd::{FlipDirection, ResampleFilter};
use std::process::ExitCode;

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

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 4 {
        eprintln!("usage: ffpsd-example <in.psd> <picture> <out.psd>");
        return ExitCode::from(2);
    }

    match run(&args[1], &args[2], &args[3]) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("{error}");
            ExitCode::FAILURE
        }
    }
}
