"""What a document takes and keeps through a save."""

import threading

import ffpsd
import numpy as np
import pytest
from conftest import BACKGROUND_COPY_NAME, GRAYSCALE_PSD, RGB_PSD, flat
from ffpsd import fourcc


def round_trip(doc):
    return ffpsd.Document.from_bytes(doc.to_bytes(ffpsd.Compression.RLE_OR_RAW))


def test_new_document_keeps_its_layers_through_a_save():
    doc = ffpsd.Document(4, 3)
    assert (doc.width, doc.height, doc.channel_count, doc.color_mode) == (4, 3, 3, ffpsd.ColorMode.RGB)

    pixels = flat(2, 2, [10, 20, 30, 255])
    layer = doc.add_layer("Пиксели", pixels, 1, 2)
    layer.opacity = 128
    layer.visible = False
    layer.clipped = True
    layer.blend_key = fourcc("mul ")
    doc.add_layer("Пустой")

    read = round_trip(doc)
    assert read.layer_count == 2
    layer = read.layer(0)
    assert layer.name == "Пиксели"
    assert np.array_equal(layer.pixels(), pixels)
    assert (layer.bounds.top, layer.bounds.left) == (1, 2)
    assert (layer.opacity, layer.visible, layer.clipped) == (128, False, True)
    assert layer.blend_key == fourcc("mul ")
    assert read.layer(1).bounds.width == 0


def test_layers_change_in_place():
    doc = ffpsd.Document(8, 8, ffpsd.ColorMode.GRAYSCALE)
    layer = doc.add_layer("a", flat(4, 2, [50, 255]))
    layer.name = "b"
    layer.set_position(3, 1)
    layer.resize(2, 4, ffpsd.ResampleFilter.NEAREST)
    layer.rotate(ffpsd.Rotation.ROTATE_90)
    layer.flip(ffpsd.FlipDirection.HORIZONTAL)
    assert layer.name == "b"
    assert (layer.bounds.width, layer.bounds.height) == (4, 2)

    layer.set_pixels(flat(1, 1, [7, 255]))
    assert np.array_equal(layer.pixels(), flat(1, 1, [7, 255]))
    assert layer.pixels_info().channel_count == 2

    # One plane may come as (height, width).
    layer.set_mask(np.zeros((2, 2), np.uint8), 1, 1, 255)
    mask = layer.mask()
    assert (mask.bounds.top, mask.bounds.left, mask.default_color) == (1, 1, 255)
    assert layer.remove_mask()
    assert layer.mask() is None
    assert not layer.remove_mask()


def test_stack_takes_copies_groups_and_moves():
    doc = ffpsd.Document(4, 4)
    doc.add_layer("bottom", flat(4, 4, [1, 2, 3, 255]))
    doc.duplicate_layer(0).name = "copy"
    group = doc.add_group("group")
    group.group_open = False
    assert group.kind == ffpsd.LayerKind.GROUP_CLOSED

    # Stored bottom to top, a group's end marker comes before its header.
    kinds = [layer.kind for layer in doc.layers]
    assert kinds == [ffpsd.LayerKind.RASTER, ffpsd.LayerKind.RASTER, ffpsd.LayerKind.GROUP_END, ffpsd.LayerKind.GROUP_CLOSED]

    copy = doc.layer(1)
    doc.move_layer(1, 2)
    assert copy.index == 2 and copy.name == "copy"
    doc.remove_layer(2)
    assert doc.layer_count == 3
    with pytest.raises(ffpsd.OutOfRangeError):
        doc.layer(9)

    # A layer of another document of the same kind.
    source = ffpsd.Document.open(RGB_PSD)
    target = ffpsd.Document(1890, 1417)
    target.add_layer_copy(source.layer(1))
    assert target.layer(0).name == BACKGROUND_COPY_NAME
    assert np.array_equal(target.layer(0).pixels(), source.layer(1).pixels())


def test_removed_layer_raises_instead_of_crashing():
    doc = ffpsd.Document(4, 4)
    removed = doc.add_layer("a")
    kept = doc.add_layer("b")
    doc.remove_layer(0)
    with pytest.raises(ffpsd.InvalidOperationError):
        removed.name
    with pytest.raises(RuntimeError):
        removed.opacity = 1
    assert "removed" in repr(removed)

    # A new layer may take the address of the removed one, and is still another layer.
    added = doc.add_layer("c")
    assert added != removed
    with pytest.raises(ffpsd.InvalidOperationError):
        removed.pixels()
    assert kept.name == "b"


def test_background_is_set_and_unset():
    doc = ffpsd.Document(4, 4)
    doc.add_background_layer("Фон", flat(4, 4, [9, 9, 9]))
    assert doc.layer(0).is_background
    assert doc.unset_background_layer()
    assert not doc.unset_background_layer()
    doc.set_background_layer(0)
    assert round_trip(doc).layer(0).is_background


def test_levels_layer_keeps_its_records():
    doc = ffpsd.Document(4, 4)
    levels = ffpsd.Levels(channels=[ffpsd.LevelsChannel(), ffpsd.LevelsChannel()])
    levels.channels[1].input_floor = 10
    levels.channels[1].input_ceiling = 240
    levels.channels[1].output_floor = 5
    levels.channels[1].output_ceiling = 250
    levels.channels[1].gamma = 1.5
    doc.add_levels_layer("Уровни", levels)
    doc.add_levels_layer("Ничего")

    read = round_trip(doc)
    assert list(read.layer(0).levels.channels)[:2] == list(levels.channels)
    assert read.layer(1).adjustment_key == fourcc("levl")

    layer = read.layer(1)
    layer.levels = levels
    assert layer.levels.channels[1] == levels.channels[1]
    with pytest.raises(ffpsd.InvalidArgumentError):
        layer.levels = ffpsd.Levels(channels=[ffpsd.LevelsChannel(gamma=0.0)])


def test_resources_and_blocks_round_trip():
    doc = ffpsd.Document(4, 4)
    resource = ffpsd.ImageResource(4000, "свой", b"\x01\x02\x03")
    doc.set_image_resource(resource)
    block = ffpsd.TaggedBlock(fourcc("Txt2"), bytes(8))
    doc.set_tagged_block(block)
    layer = doc.add_layer("a")
    layer_block = ffpsd.TaggedBlock(fourcc("lclr"), bytes([0, 1, 0, 0, 0, 0, 0, 0]))
    layer.set_tagged_block(layer_block)

    info = doc.version_info
    info.writer_name = "ffpsd-python"
    doc.version_info = info
    resolution = doc.resolution_info
    resolution.horizontal = resolution.vertical = 150.0
    doc.resolution_info = resolution

    read = round_trip(doc)
    assert read.find_image_resource(4000) == resource
    assert read.find_tagged_block(fourcc("Txt2")) == block
    assert read.layer(0).find_tagged_block(fourcc("lclr")) == layer_block
    assert read.version_info.writer_name == "ffpsd-python"
    assert read.resolution_info.horizontal == 150.0

    assert read.remove_image_resource(4000)
    assert read.remove_tagged_block(fourcc("Txt2"))
    assert read.layer(0).remove_tagged_block(fourcc("lclr"))
    assert read.find_image_resource(4000) is None
    assert read.find_tagged_block(fourcc("Txt2")) is None


def test_canvas_changes_and_composite_is_kept():
    doc = ffpsd.Document(4, 2)
    doc.add_layer("a", flat(4, 2, [1, 2, 3, 255]))
    doc.set_merged_image(flat(4, 2, [1, 2, 3]))
    assert doc.has_real_merged_data
    assert np.array_equal(round_trip(doc).merged_image(), flat(4, 2, [1, 2, 3]))

    doc.resize_canvas(6, 4, ffpsd.Anchor.CENTER)
    assert (doc.width, doc.height) == (6, 4)
    doc.rotate_canvas(ffpsd.Rotation.ROTATE_90)
    assert (doc.width, doc.height) == (4, 6)
    doc.flip_canvas(ffpsd.FlipDirection.VERTICAL)
    doc.resize(8, 12, ffpsd.ResampleFilter.BICUBIC)
    assert (doc.width, doc.height) == (8, 12)

    doc.convert_color_mode(ffpsd.ColorMode.GRAYSCALE)
    assert (doc.color_mode, doc.channel_count) == (ffpsd.ColorMode.GRAYSCALE, 1)
    doc.psb = True
    assert round_trip(doc).psb


def test_files_are_written_and_errors_say_why(temp):
    doc = ffpsd.Document.open(GRAYSCALE_PSD)
    path = temp("written.psd")
    doc.save(path)
    assert ffpsd.Document.open(path).layer_count == 2

    with pytest.raises(ffpsd.IoError) as missing:
        ffpsd.Document.open(temp("missing.psd"))
    assert isinstance(missing.value, OSError) and str(missing.value)
    with pytest.raises(ffpsd.InvalidFileError):
        ffpsd.Document.from_bytes(b"not a psd")
    with pytest.raises(ffpsd.InvalidArgumentError):
        ffpsd.Document(4, 4, ffpsd.ColorMode.MULTICHANNEL)
    with pytest.raises(ffpsd.InvalidArgumentError, match="uint8, uint16 or float32"):
        ffpsd.Document(4, 4).add_layer("a", np.zeros((3, 2, 2)))


def test_pixels_come_planar_and_go_in_transposed():
    doc = ffpsd.Document(3, 2)
    interleaved = np.zeros((2, 3, 3), np.uint8)
    interleaved[..., 0] = 200
    layer = doc.add_layer("a", interleaved.transpose(2, 0, 1))

    # A layer gets transparency, opaque where the picture had none.
    pixels = layer.pixels()
    assert pixels.shape == (4, 2, 3) and pixels.dtype == np.uint8 and pixels.flags.c_contiguous
    assert pixels[0].min() == 200 and pixels[1:3].max() == 0 and pixels[3].min() == 255
    assert pixels.transpose(1, 2, 0).shape == (2, 3, 4)


def test_documents_open_in_threads():
    results = []
    threads = [threading.Thread(target=lambda: results.append(ffpsd.Document.open(GRAYSCALE_PSD).layer_count)) for _ in range(4)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    assert results == [2] * 4
