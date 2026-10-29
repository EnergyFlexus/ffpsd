//! What a document takes and keeps through a save.
mod common;

use common::*;
use ffpsd::{
    fourcc, Anchor, ColorMode, Compression, Document, ErrorKind, FlipDirection, ImageResource, LayerKind, Levels,
    LevelsChannel, ResampleFilter, Rotation, TaggedBlock,
};

fn round_trip(doc: &Document) -> Document {
    Document::open_from_memory(&doc.save_to_memory(Compression::RleOrRaw).unwrap()).unwrap()
}

#[test]
fn new_document_keeps_its_layers_through_a_save() {
    let mut doc = Document::new(4, 3, ColorMode::Rgb, 8).unwrap();
    assert_eq!((doc.width(), doc.height(), doc.channel_count()), (4, 3, 3));

    let pixels = flat(2, 2, &[10, 20, 30, 255]);
    let mut layer = doc.add_layer("Пиксели", Some(pixels.view()), 1, 2).unwrap();
    layer.set_opacity(128).unwrap();
    layer.set_visible(false).unwrap();
    layer.set_clipped(true).unwrap();
    layer.set_blend_key(fourcc(b"mul ")).unwrap();
    doc.add_layer("Пустой", None, 0, 0).unwrap();

    let read = round_trip(&doc);
    assert_eq!(read.layer_count(), 2);
    let layer = read.layer(0).unwrap();
    assert_eq!(layer.name(), "Пиксели");
    assert_eq!(layer.pixels().unwrap(), pixels);
    assert_eq!((layer.bounds().top, layer.bounds().left), (1, 2));
    assert_eq!(layer.opacity(), 128);
    assert!(!layer.is_visible());
    assert!(layer.is_clipped());
    assert_eq!(layer.blend_key(), fourcc(b"mul "));
    assert_eq!(read.layer(1).unwrap().bounds().width(), 0);
}

#[test]
fn layers_change_in_place() {
    let mut doc = Document::new(8, 8, ColorMode::Grayscale, 8).unwrap();
    let mut layer = doc.add_layer("a", Some(flat(4, 2, &[50, 255]).view()), 0, 0).unwrap();
    layer.set_name("b").unwrap();
    layer.set_position(3, 1).unwrap();
    layer.resize(2, 4, ResampleFilter::Nearest).unwrap();
    layer.rotate(Rotation::Rotate90).unwrap();
    layer.flip(FlipDirection::Horizontal).unwrap();
    assert_eq!(layer.name(), "b");
    assert_eq!((layer.bounds().width(), layer.bounds().height()), (4, 2));

    layer.set_pixels(flat(1, 1, &[7, 255]).view()).unwrap();
    assert_eq!(layer.pixels().unwrap(), flat(1, 1, &[7, 255]));
    assert_eq!(layer.pixels_info().unwrap().channel_count, 2);

    layer.set_mask(flat(2, 2, &[0]).view(), 1, 1, 255).unwrap();
    let mask = layer.mask().unwrap().unwrap();
    assert_eq!((mask.bounds.top, mask.bounds.left, mask.default_color), (1, 1, 255));
    layer.remove_mask().unwrap();
    assert_eq!(layer.mask().unwrap(), None);
}

#[test]
fn stack_takes_copies_groups_and_moves() {
    let mut doc = Document::new(4, 4, ColorMode::Rgb, 8).unwrap();
    doc.add_layer("bottom", Some(flat(4, 4, &[1, 2, 3, 255]).view()), 0, 0)
        .unwrap();
    doc.duplicate_layer(0).unwrap().set_name("copy").unwrap();
    let mut group = doc.add_group("group").unwrap();
    group.set_group_open(false).unwrap();
    assert_eq!(group.kind(), LayerKind::GroupClosed);

    // Stored bottom to top, a group's end marker comes before its header.
    let kinds: Vec<LayerKind> = doc.layers().map(|layer| layer.kind()).collect();
    assert_eq!(
        kinds,
        [
            LayerKind::Raster,
            LayerKind::Raster,
            LayerKind::GroupEnd,
            LayerKind::GroupClosed
        ]
    );

    doc.move_layer(1, 2).unwrap();
    let names: Vec<String> = doc.layers().map(|layer| layer.name()).collect();
    assert_eq!(names[2], "copy");
    doc.remove_layer(2).unwrap();
    assert_eq!(doc.layer_count(), 3);
    assert_eq!(doc.layer(9).unwrap_err().kind(), ErrorKind::OutOfRange);

    // A layer of another document of the same kind.
    let source = Document::open(data(RGB_PSD)).unwrap();
    let mut target = Document::new(1890, 1417, ColorMode::Rgb, 8).unwrap();
    target.add_layer_copy(&source.layer(1).unwrap()).unwrap();
    assert_eq!(target.layer(0).unwrap().name(), BACKGROUND_COPY_NAME);
    assert_eq!(
        target.layer(0).unwrap().pixels().unwrap(),
        source.layer(1).unwrap().pixels().unwrap()
    );
}

#[test]
fn background_is_set_and_unset() {
    let mut doc = Document::new(4, 4, ColorMode::Rgb, 8).unwrap();
    doc.add_background_layer("Фон", Some(flat(4, 4, &[9, 9, 9]).view()))
        .unwrap();
    assert!(doc.layer(0).unwrap().is_background());
    assert!(doc.unset_background_layer().unwrap());
    assert!(!doc.unset_background_layer().unwrap());
    doc.set_background_layer(0).unwrap();
    assert!(round_trip(&doc).layer(0).unwrap().is_background());
}

#[test]
fn levels_layer_keeps_its_records() {
    let mut doc = Document::new(4, 4, ColorMode::Rgb, 8).unwrap();
    let mut levels = Levels {
        channels: vec![LevelsChannel::IDENTITY; 2],
    };
    levels.channels[1] = LevelsChannel {
        input_floor: 10,
        input_ceiling: 240,
        output_floor: 5,
        output_ceiling: 250,
        gamma: 1.5,
    };
    doc.add_levels_layer("Уровни", Some(&levels)).unwrap();
    doc.add_levels_layer("Ничего", None).unwrap();

    let read = round_trip(&doc);
    assert_eq!(
        read.layer(0).unwrap().levels().unwrap().unwrap().channels[..2],
        levels.channels[..]
    );
    assert_eq!(read.layer(1).unwrap().adjustment_key(), Some(fourcc(b"levl")));

    let mut doc = read;
    let mut layer = doc.layer_mut(1).unwrap();
    layer.set_levels(&levels).unwrap();
    assert_eq!(layer.levels().unwrap().unwrap().channels[1], levels.channels[1]);

    let bad = Levels {
        channels: vec![LevelsChannel {
            gamma: 0.0,
            ..LevelsChannel::IDENTITY
        }],
    };
    assert_eq!(layer.set_levels(&bad).unwrap_err().kind(), ErrorKind::InvalidArgument);
}

#[test]
fn resources_and_blocks_round_trip() {
    let mut doc = Document::new(4, 4, ColorMode::Rgb, 8).unwrap();
    let resource = ImageResource {
        id: 4000,
        name: "свой".into(),
        data: vec![1, 2, 3],
    };
    doc.set_image_resource(&resource).unwrap();
    let block = TaggedBlock {
        signature: fourcc(b"8BIM"),
        key: fourcc(b"Txt2"),
        data: vec![9; 8],
    };
    doc.set_tagged_block(&block).unwrap();
    let mut layer = doc.add_layer("a", None, 0, 0).unwrap();
    let layer_block = TaggedBlock {
        signature: fourcc(b"8BIM"),
        key: fourcc(b"lclr"),
        data: vec![0, 1, 0, 0, 0, 0, 0, 0],
    };
    layer.set_tagged_block(&layer_block).unwrap();

    let mut info = doc.version_info().unwrap();
    info.writer_name = "ffpsd-rust".into();
    doc.set_version_info(&info).unwrap();
    let mut resolution = doc.resolution_info().unwrap();
    resolution.horizontal = 150.0;
    resolution.vertical = 150.0;
    doc.set_resolution_info(&resolution).unwrap();

    let mut read = round_trip(&doc);
    assert_eq!(read.find_image_resource(4000).unwrap(), Some(resource));
    assert_eq!(read.find_tagged_block(fourcc(b"Txt2")).unwrap(), Some(block));
    assert_eq!(
        read.layer(0).unwrap().find_tagged_block(fourcc(b"lclr")).unwrap(),
        Some(layer_block)
    );
    assert_eq!(read.version_info().unwrap().writer_name, "ffpsd-rust");
    assert_eq!(read.resolution_info().unwrap().horizontal, 150.0);

    read.remove_image_resource(4000).unwrap();
    read.remove_tagged_block(fourcc(b"Txt2")).unwrap();
    read.layer_mut(0).unwrap().remove_tagged_block(fourcc(b"lclr")).unwrap();
    assert_eq!(read.find_image_resource(4000).unwrap(), None);
    assert_eq!(read.find_tagged_block(fourcc(b"Txt2")).unwrap(), None);
}

#[test]
fn canvas_changes_and_composite_is_kept() {
    let mut doc = Document::new(4, 2, ColorMode::Rgb, 8).unwrap();
    doc.add_layer("a", Some(flat(4, 2, &[1, 2, 3, 255]).view()), 0, 0)
        .unwrap();
    doc.set_merged_image(flat(4, 2, &[1, 2, 3]).view()).unwrap();
    assert!(doc.has_real_merged_data());
    assert_eq!(round_trip(&doc).merged_image().unwrap(), flat(4, 2, &[1, 2, 3]));

    doc.resize_canvas(6, 4, Anchor::Center).unwrap();
    assert_eq!((doc.width(), doc.height()), (6, 4));
    doc.rotate_canvas(Rotation::Rotate90).unwrap();
    assert_eq!((doc.width(), doc.height()), (4, 6));
    doc.flip_canvas(FlipDirection::Vertical).unwrap();
    doc.resize(8, 12, ResampleFilter::Bicubic).unwrap();
    assert_eq!((doc.width(), doc.height()), (8, 12));

    doc.convert_color_mode(ColorMode::Grayscale).unwrap();
    assert_eq!((doc.color_mode(), doc.channel_count()), (ColorMode::Grayscale, 1));
    doc.set_psb(true).unwrap();
    assert!(round_trip(&doc).is_psb());
}

#[test]
fn files_are_written_and_errors_say_why() {
    let doc = Document::open(data(GRAYSCALE_PSD)).unwrap();
    let path = temp("written.psd");
    doc.save(&path, Compression::Default).unwrap();
    assert_eq!(Document::open(&path).unwrap().layer_count(), 2);
    std::fs::remove_file(&path).unwrap();

    let missing = Document::open(temp("missing.psd")).unwrap_err();
    assert_eq!(missing.kind(), ErrorKind::Io);
    assert!(!missing.to_string().is_empty());
    assert_eq!(
        Document::open_from_memory(b"not a psd").unwrap_err().kind(),
        ErrorKind::InvalidFile
    );
    assert_eq!(
        Document::open("a\0b.psd").unwrap_err().kind(),
        ErrorKind::InvalidArgument
    );
    assert_eq!(
        Document::new(4, 4, ColorMode::Multichannel, 8).unwrap_err().kind(),
        ErrorKind::InvalidArgument
    );
}

#[test]
fn document_moves_to_another_thread() {
    let doc = Document::open(data(GRAYSCALE_PSD)).unwrap();
    let count = std::thread::spawn(move || doc.layer_count()).join().unwrap();
    assert_eq!(count, 2);
}
