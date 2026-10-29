//! Opens a PSD, prints what it holds, copies the top layer, adds a gradient and saves the result.
use ffpsd::{ColorMode, Compression, Document, ImageInfo, ImageView};
use std::process::ExitCode;

fn run(input: &str, output: &str) -> ffpsd::Result<()> {
    let mut doc = Document::open(input)?;

    // Layers go from the bottom up.
    println!(
        "{} x {}, {} bit, {} layers",
        doc.width(),
        doc.height(),
        doc.depth(),
        doc.layer_count()
    );
    for (index, layer) in doc.layers().enumerate() {
        let bounds = layer.bounds();
        let pixels = layer.pixels()?;

        // Planes lie one after another, so the first plane is the first width * height samples.
        let plane = pixels.info.width as usize * pixels.info.height as usize;
        let mean = match (pixels.info.depth, plane) {
            (8, 1..) => format!(
                ", mean of the first plane {}",
                pixels.data[..plane].iter().map(|&v| u64::from(v)).sum::<u64>() / plane as u64
            ),
            _ => String::new(),
        };
        println!(
            "  {index}: {}, {} x {}, {} planes{mean}",
            layer.name(),
            bounds.width(),
            bounds.height(),
            pixels.info.channel_count
        );
    }

    // Put a copy of the top layer on top; a group marker alone cannot be copied.
    doc.duplicate_layer(doc.layer_count().wrapping_sub(1))?;

    // Add a gradient over the canvas, planar as PSD keeps it: red grows to the right, green down, blue stays at half.
    let color_mode = doc.color_mode();
    let channel_count: u16 = if color_mode == ColorMode::Grayscale { 1 } else { 3 };
    let (columns, rows) = (doc.width() as usize, doc.height() as usize);
    let plane = columns * rows;
    let mut pixels = vec![128u8; plane * usize::from(channel_count)];
    for y in 0..rows {
        for x in 0..columns {
            let at = y * columns + x;
            pixels[at] = (x * 255 / columns) as u8;
            if channel_count == 3 {
                pixels[plane + at] = (y * 255 / rows) as u8;
            }
        }
    }
    let info = ImageInfo {
        width: doc.width(),
        height: doc.height(),
        channel_count,
        depth: 8,
        color_mode,
    };
    doc.add_layer("Gradient", Some(ImageView::new(info, &pixels)), 0, 0)?;

    // Save, each channel RLE or raw, whichever is smaller.
    doc.save(output, Compression::RleOrRaw)?;
    println!("saved {} layers to {output}", doc.layer_count());
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
        Err(error) => {
            eprintln!("{error}");
            ExitCode::FAILURE
        }
    }
}
