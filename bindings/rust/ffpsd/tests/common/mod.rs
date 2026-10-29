//! What the tests share: the files of tests/data, whose values come from independent dumps.
#![allow(dead_code)]
use ffpsd::{ColorMode, Image, ImageInfo};
use std::path::PathBuf;

pub const GRAYSCALE_PSD: &str = "photoshop/grayscale_two_layers.psd";
pub const RGB_PSD: &str = "photoshop/rgb_two_layers.psd";
pub const RGB_LEVELS_PSD: &str = "photoshop/rgb_levels.psd";
pub const RGB_MASKS_PSD: &str = "photoshop/rgb_masks.psd";
pub const RGBA_PNG: &str = "generated/rgba_8bit.png";
pub const RGBA_WEBP: &str = "generated/rgba_lossless.webp";
pub const QUADRANTS_JPEG: &str = "generated/rgb_quadrants.jpg";

pub const BACKGROUND_NAME: &str = "Фон";
pub const BACKGROUND_COPY_NAME: &str = "Фон копия";
pub const LEVELS_NAME: &str = "Уровни 1";

/// The planes of RGBA_PNG and RGBA_WEBP, 3 x 2: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
pub const RGBA_PLANES: [u8; 24] = [
    255, 0, 0, 255, 0, 128, // R
    0, 255, 0, 255, 0, 128, // G
    0, 0, 255, 255, 0, 128, // B
    255, 128, 0, 255, 255, 64, // A
];

pub fn data(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../../tests/data")
        .join(name)
}

/// A file in the temporary folder that no other test writes.
pub fn temp(name: &str) -> PathBuf {
    std::env::temp_dir().join(format!("ffpsd-rust-{}-{name}", std::process::id()))
}

pub fn plane_sum(image: &Image, plane: usize) -> u64 {
    let size = image.info.width as usize * image.info.height as usize;
    image.data[plane * size..(plane + 1) * size]
        .iter()
        .map(|&value| u64::from(value))
        .sum()
}

pub fn rgba_info() -> ImageInfo {
    ImageInfo {
        width: 3,
        height: 2,
        channel_count: 4,
        depth: 8,
        color_mode: ColorMode::Rgb,
    }
}

/// Planes of one value each, 8 bit.
pub fn flat(width: u32, height: u32, values: &[u8]) -> Image {
    let size = width as usize * height as usize;
    let data = values
        .iter()
        .flat_map(|&value| std::iter::repeat(value).take(size))
        .collect();
    let color_mode = if values.len() < 3 {
        ColorMode::Grayscale
    } else {
        ColorMode::Rgb
    };
    Image {
        info: ImageInfo {
            width,
            height,
            channel_count: values.len() as u16,
            depth: 8,
            color_mode,
        },
        data,
    }
}
