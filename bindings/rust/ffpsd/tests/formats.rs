//! PNG, JPEG and WebP in and out of the planes a layer takes.
mod common;

use common::*;
use ffpsd::formats::*;
use ffpsd::{ColorMode, ErrorKind, Format, Image, PngCompression};

fn rgba() -> Image {
    Image {
        info: rgba_info(),
        data: RGBA_PLANES.to_vec(),
    }
}

#[test]
fn every_format_is_in_the_default_build() {
    for format in [Format::Png, Format::Jpeg, Format::Webp] {
        assert!(is_format_supported(format), "{format:?}");
    }
}

#[test]
fn png_and_webp_load_known_pixels() {
    assert_eq!(load_png(data(RGBA_PNG), ColorMode::Rgb, 8).unwrap(), rgba());
    assert_eq!(load_webp(data(RGBA_WEBP), ColorMode::Rgb, 8).unwrap(), rgba());
    assert_eq!(load_picture(data(RGBA_PNG), ColorMode::Rgb, 8).unwrap(), rgba());

    // As gray: 0.2126, 0.7152 and 0.0722 of 255, rounded, then the alpha.
    let gray = load_png(data(RGBA_PNG), ColorMode::Grayscale, 8).unwrap();
    assert_eq!(gray.data, [54, 182, 18, 255, 0, 128, 255, 128, 0, 255, 255, 64]);
}

#[test]
fn lossless_formats_round_trip() {
    let image = rgba();
    let png = save_png_to_memory(image.view(), PngCompression::Smallest).unwrap();
    assert_eq!(load_png_from_memory(&png, ColorMode::Rgb, 8).unwrap(), image);
    let webp = save_webp_to_memory(image.view(), 90, true).unwrap();
    assert_eq!(load_webp_from_memory(&webp, ColorMode::Rgb, 8).unwrap(), image);
    assert_eq!(load_picture_from_memory(&webp, ColorMode::Rgb, 8).unwrap(), image);

    let path = temp("rgba.png");
    save_png(image.view(), &path, PngCompression::default()).unwrap();
    assert_eq!(load_png(&path, ColorMode::Rgb, 8).unwrap(), image);
    std::fs::remove_file(&path).unwrap();
}

#[test]
fn jpeg_keeps_the_shape_and_drops_alpha() {
    let quadrants = load_jpeg(data(QUADRANTS_JPEG), ColorMode::Rgb, 8, true).unwrap();
    assert_eq!(quadrants.info.channel_count, 3);

    let jpeg = save_jpeg_to_memory(rgba().view(), 95).unwrap();
    let read = load_jpeg_from_memory(&jpeg, ColorMode::Rgb, 8, false).unwrap();
    assert_eq!((read.info.width, read.info.height, read.info.channel_count), (3, 2, 3));

    let path = temp("rgba.jpg");
    save_jpeg(rgba().view(), &path, 80).unwrap();
    save_webp(rgba().view(), temp("rgba.webp"), 80, false).unwrap();
    assert_eq!(load_picture(&path, ColorMode::Rgb, 8).unwrap().info.channel_count, 3);
    std::fs::remove_file(&path).unwrap();
    std::fs::remove_file(temp("rgba.webp")).unwrap();
}

#[test]
fn bad_input_is_an_error() {
    assert_eq!(
        load_png_from_memory(b"not a png", ColorMode::Rgb, 8)
            .unwrap_err()
            .kind(),
        ErrorKind::InvalidFile
    );
    assert_eq!(
        save_jpeg_to_memory(rgba().view(), 0).unwrap_err().kind(),
        ErrorKind::InvalidArgument
    );
    let short = ffpsd::ImageView::new(rgba_info(), &RGBA_PLANES[..10]);
    assert_eq!(
        save_png_to_memory(short, PngCompression::Fastest).unwrap_err().kind(),
        ErrorKind::InvalidArgument
    );
}
