//! Photoshop files read as their dumps say.
mod common;

use common::*;
use ffpsd::{fourcc, ChannelCompression, ColorMode, Document, ErrorKind, LayerKind, Rect};

#[test]
fn grayscale_file_reads_as_the_dump_says() {
    let doc = Document::open(data(GRAYSCALE_PSD)).unwrap();
    assert_eq!((doc.width(), doc.height(), doc.channel_count(), doc.depth()), (836, 882, 1, 8));
    assert_eq!(doc.color_mode(), ColorMode::Grayscale);
    assert!(!doc.is_psb());

    let names: Vec<String> = doc.layers().map(|layer| layer.name()).collect();
    assert_eq!(names, [BACKGROUND_NAME, BACKGROUND_COPY_NAME]);
    for layer in doc.layers() {
        assert_eq!(layer.kind(), LayerKind::Raster);
        assert!(layer.is_visible());
        assert_eq!(layer.opacity(), 255);
        assert_eq!(layer.blend_key(), fourcc(b"norm"));
        assert_eq!((layer.bounds().width(), layer.bounds().height()), (836, 882));
    }
    assert!(doc.layer(0).unwrap().is_background());

    // A background has no transparency; the layer above it has, as the last plane.
    let background = doc.layer(0).unwrap().pixels().unwrap();
    let copy = doc.layer(1).unwrap().pixels().unwrap();
    assert_eq!((background.info.channel_count, copy.info.channel_count), (1, 2));
    assert_eq!(plane_sum(&background, 0), 137735731);
    assert_eq!(plane_sum(&copy, 0), 137565364);
    assert_eq!(plane_sum(&copy, 1), 188024760);
}

#[test]
fn rgb_file_reads_as_the_dump_says() {
    let doc = Document::open(data(RGB_PSD)).unwrap();
    assert_eq!(doc.image_resource_count(), 29);

    // Set up in pixels per centimeter, so Photoshop stores 0x012BFFFE, just under 300 ppi.
    let resolution = doc.resolution_info().unwrap();
    assert_eq!(resolution.horizontal, f64::from(0x012BFFFE) / 65536.0);
    assert_eq!(resolution.vertical, f64::from(0x012BFFFE) / 65536.0);
    assert_eq!((resolution.horizontal_unit, resolution.width_unit), (2, 2));

    let version = doc.version_info().unwrap();
    assert_eq!(version.version, 1);
    assert!(version.has_real_merged_data);
    assert_eq!(version.writer_name, "Adobe Photoshop");
    assert_eq!(version.reader_name, "Adobe Photoshop 2026");

    // 1044, the last layer id handed out, and 1024, the target layer counted from the bottom.
    let seed = doc.find_image_resource(1044).unwrap().unwrap();
    assert_eq!(seed.data, [0, 0, 0, 2]);
    assert_eq!(doc.find_image_resource(1024).unwrap().unwrap().data, [0, 1]);
    assert_eq!(doc.find_image_resource(4242).unwrap(), None);

    let background = doc.layer(0).unwrap().pixels().unwrap();
    let copy = doc.layer(1).unwrap().pixels().unwrap();
    assert_eq!((background.info.channel_count, copy.info.channel_count), (3, 4));
    assert_eq!(plane_sum(&background, 0), 682923150);
    let sums: Vec<u64> = (0..4).map(|plane| plane_sum(&copy, plane)).collect();
    assert_eq!(sums, [183326186, 682923150, 206836566, 682923150]);

    let merged = doc.merged_image().unwrap();
    assert_eq!(merged.info, doc.merged_image_info().unwrap());
    assert_eq!((merged.info.width, merged.info.height, merged.info.channel_count), (1890, 1417, 3));
    let sums: Vec<u64> = (0..3).map(|plane| plane_sum(&merged, plane)).collect();
    assert_eq!(sums, [183326186, 682923150, 206836566]);
    assert_eq!(doc.merged_compression().unwrap(), Some(ChannelCompression::Rle));

    // The blocks after the layers, in file order.
    let blocks: Vec<(u32, usize)> = (0..doc.tagged_block_count())
        .map(|index| doc.tagged_block(index).unwrap())
        .map(|block| (block.key, block.data.len()))
        .collect();
    let expected = [
        (b"Patt", 0),
        (b"CAI ", 77),
        (b"OCIO", 170),
        (b"GenI", 84),
        (b"FMsk", 12),
        (b"cinf", 410),
    ];
    assert_eq!(blocks, expected.map(|(key, size)| (fourcc(key), size)));
    assert_eq!(doc.find_tagged_block(fourcc(b"OCIO")).unwrap().unwrap().data.len(), 170);
}

#[test]
fn levels_layer_reads_its_records() {
    let doc = Document::open(data(RGB_LEVELS_PSD)).unwrap();
    let levels = doc.layer(1).unwrap();
    assert_eq!(levels.name(), LEVELS_NAME);
    assert_eq!(levels.kind(), LayerKind::Adjustment);
    assert_eq!(levels.adjustment_key(), Some(fourcc(b"levl")));
    assert!(!levels.levels().unwrap().unwrap().channels.is_empty());

    let background = doc.layer(0).unwrap();
    assert_eq!(background.adjustment_key(), None);
    assert_eq!(background.levels().unwrap(), None);
}

#[test]
fn masks_read_and_refuse_what_photoshop_would() {
    let mut doc = Document::open(data(RGB_MASKS_PSD)).unwrap();
    assert_eq!(doc.layer(1).unwrap().mask().unwrap(), None);

    let mask = doc.layer(2).unwrap().mask().unwrap().unwrap();
    assert_eq!(
        mask.bounds,
        Rect {
            top: 0,
            left: 935,
            bottom: 1417,
            right: 1890
        }
    );
    assert_eq!(mask.default_color, 255);
    assert_eq!((mask.image.info.width, mask.image.info.height), (955, 1417));
    assert_eq!(
        (mask.image.info.channel_count, mask.image.info.color_mode),
        (1, ColorMode::Grayscale)
    );
    assert!(doc.layer(2).unwrap().channels().iter().any(|channel| channel.id == -2));

    // A layer with a vector mask keeps it as it is.
    let pattern = flat(2, 2, &[1]);
    let mut vector = doc.layer_mut(3).unwrap();
    assert_eq!(
        vector.set_mask(pattern.view(), 0, 0, 0).unwrap_err().kind(),
        ErrorKind::InvalidOperation
    );
    assert_eq!(vector.set_position(1, 1).unwrap_err().kind(), ErrorKind::InvalidOperation);

    let mut unmasked = doc.layer_mut(1).unwrap();
    let three = flat(2, 2, &[1, 2, 3]);
    assert_eq!(
        unmasked.set_mask(three.view(), 0, 0, 0).unwrap_err().kind(),
        ErrorKind::InvalidArgument
    );
    let error = unmasked.set_mask(pattern.view(), 0, 0, 7).unwrap_err();
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
    assert!(!error.message().is_empty());
}

#[test]
fn layers_iterate_from_both_ends() {
    let doc = Document::open(data(GRAYSCALE_PSD)).unwrap();
    let layers = doc.layers();
    assert_eq!(layers.len(), 2);
    let names: Vec<String> = doc.layers().rev().map(|layer| layer.name()).collect();
    assert_eq!(names, [BACKGROUND_COPY_NAME, BACKGROUND_NAME]);
    let layer = doc.layer(1).unwrap();
    assert_eq!(doc.find_layer_index(&layer), Some(1));

    let other = Document::open(data(GRAYSCALE_PSD)).unwrap();
    assert_eq!(doc.find_layer_index(&other.layer(1).unwrap()), None);
}

#[test]
fn version_is_the_library_one() {
    let version = ffpsd::version();
    assert_eq!(version.split('.').count(), 3, "{version}");
}
