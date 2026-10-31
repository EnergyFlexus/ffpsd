//! Opens a PSD, adds a copy of the top layer, a picture with a mask and a Levels layer, grows the canvas and saves the result.
use ffpsd::formats::{load_picture, save_png_to_memory};
use ffpsd::{
    fourcc, Anchor, ColorMode, Compression, Document, FlipDirection, ImageInfo, ImageView, Levels, LevelsChannel, PngCompression,
    ResampleFilter,
};
use std::process::ExitCode;

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
