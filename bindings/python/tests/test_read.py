"""Photoshop files read as their dumps say."""

import ffpsd
import pytest
from conftest import (
    BACKGROUND_COPY_NAME,
    BACKGROUND_NAME,
    GRAYSCALE_PSD,
    LEVELS_NAME,
    RGB_LEVELS_PSD,
    RGB_MASKS_PSD,
    RGB_PSD,
    flat,
    found,
    plane_sums,
)
from ffpsd import fourcc


def test_grayscale_file_reads_as_the_dump_says():
    doc = ffpsd.Document.open(GRAYSCALE_PSD)
    assert (doc.width, doc.height, doc.channel_count, doc.depth) == (836, 882, 1, 8)
    assert doc.color_mode == ffpsd.ColorMode.GRAYSCALE
    assert not doc.psb

    assert [layer.name for layer in doc.layers] == [BACKGROUND_NAME, BACKGROUND_COPY_NAME]
    for layer in doc.layers:
        assert layer.kind == ffpsd.LayerKind.RASTER
        assert layer.visible
        assert layer.opacity == 255
        assert layer.blend_key == fourcc("norm")
        assert (layer.bounds.width, layer.bounds.height) == (836, 882)
    assert doc.layer(0).is_background

    # A background has no transparency; the layer above it has, as the last plane.
    background = doc.layer(0).pixels()
    copy = doc.layer(1).pixels()
    assert (background.shape, copy.shape) == ((1, 882, 836), (2, 882, 836))
    assert plane_sums(background) == [137735731]
    assert plane_sums(copy) == [137565364, 188024760]


def test_rgb_file_reads_as_the_dump_says():
    doc = ffpsd.Document.open(str(RGB_PSD))
    assert len(doc.image_resources) == 29

    # Set up in pixels per centimeter, so Photoshop stores 0x012BFFFE, just under 300 ppi.
    resolution = doc.resolution_info
    assert resolution.horizontal == resolution.vertical == 0x012BFFFE / 65536
    assert (resolution.horizontal_unit, resolution.width_unit) == (2, 2)

    version = doc.version_info
    assert version.version == 1
    assert version.has_real_merged_data
    assert (version.writer_name, version.reader_name) == ("Adobe Photoshop", "Adobe Photoshop 2026")

    # 1044, the last layer id handed out, and 1024, the target layer counted from the bottom.
    assert found(doc.find_image_resource(1044)).data == bytes([0, 0, 0, 2])
    assert found(doc.find_image_resource(1024)).data == bytes([0, 1])
    assert doc.find_image_resource(4242) is None

    background = doc.layer(0).pixels()
    copy = doc.layer(1).pixels()
    assert (len(background), len(copy)) == (3, 4)
    assert plane_sums(background)[0] == 682923150
    assert plane_sums(copy) == [183326186, 682923150, 206836566, 682923150]

    merged = found(doc.merged_image())
    info = found(doc.merged_image_info())
    assert merged.shape == (info.channel_count, info.height, info.width) == (3, 1417, 1890)
    assert plane_sums(merged) == [183326186, 682923150, 206836566]
    assert doc.merged_compression == ffpsd.ChannelCompression.RLE

    # The blocks after the layers, in file order.
    expected = [("Patt", 0), ("CAI ", 77), ("OCIO", 170), ("GenI", 84), ("FMsk", 12), ("cinf", 410)]
    assert [(block.key, len(block.data)) for block in doc.tagged_blocks] == [(fourcc(k), n) for k, n in expected]
    assert len(found(doc.find_tagged_block(fourcc("OCIO"))).data) == 170


def test_levels_layer_reads_its_records():
    doc = ffpsd.Document.open(RGB_LEVELS_PSD)
    levels = doc.layer(1)
    assert levels.name == LEVELS_NAME
    assert levels.kind == ffpsd.LayerKind.ADJUSTMENT
    assert levels.adjustment_key == fourcc("levl")
    assert len(found(levels.levels).channels) > 0

    background = doc.layer(0)
    assert background.adjustment_key is None
    assert background.levels is None


def test_masks_read_and_refuse_what_photoshop_would():
    doc = ffpsd.Document.open(RGB_MASKS_PSD)
    assert doc.layer(1).mask() is None

    mask = found(doc.layer(2).mask())
    assert mask.bounds == ffpsd.Rect(top=0, left=935, bottom=1417, right=1890)
    assert mask.default_color == 255
    assert mask.image.shape == (1, 1417, 955)
    assert any(channel.id == -2 for channel in doc.layer(2).channels)

    # A layer with a vector mask keeps it as it is.
    vector = doc.layer(3)
    with pytest.raises(ffpsd.InvalidOperationError):
        vector.set_mask(flat(2, 2, [1]), 0, 0, 0)
    with pytest.raises(ffpsd.InvalidOperationError):
        vector.set_position(1, 1)

    unmasked = doc.layer(1)
    with pytest.raises(ffpsd.InvalidArgumentError):
        unmasked.set_mask(flat(2, 2, [1, 2, 3]))
    with pytest.raises(ValueError, match="."):
        unmasked.set_mask(flat(2, 2, [1]), default_color=7)


def test_layers_index_from_both_ends():
    doc = ffpsd.Document.open(GRAYSCALE_PSD)
    assert doc.layer_count == len(doc.layers) == 2
    assert [layer.name for layer in reversed(doc.layers)] == [BACKGROUND_COPY_NAME, BACKGROUND_NAME]
    assert doc.layer(-1) == doc.layer(1)
    assert doc.find_layer_index(doc.layer(1)) == doc.layer(1).index == 1

    other = ffpsd.Document.open(GRAYSCALE_PSD)
    assert doc.find_layer_index(other.layer(1)) is None
    with pytest.raises(IndexError):
        doc.layer(2)


def test_version_is_the_library_one():
    assert ffpsd.__version__ == ffpsd.version()
    assert len(ffpsd.version().split(".")) == 3
