"""Pillow images in and out of the planes ffpsd takes."""

import ffpsd
import numpy as np
import pytest
from conftest import RGBA_PLANES

Image = pytest.importorskip("PIL.Image")


def interleaved(planes):
    return np.ascontiguousarray(planes.transpose(1, 2, 0))


def test_rgba_comes_in_and_goes_out_as_it_was():
    image = Image.fromarray(interleaved(RGBA_PLANES))
    planes = ffpsd.from_pil(image)
    assert planes.dtype == np.uint8 and planes.flags.c_contiguous
    assert np.array_equal(planes, RGBA_PLANES)

    back = ffpsd.to_pil(planes)
    assert back.mode == "RGBA"
    assert np.array_equal(np.asarray(back), interleaved(RGBA_PLANES))


def test_modes_pillow_has_and_ffpsd_does_not_are_converted():
    # A palette with a transparent entry, as a GIF has, comes in as RGBA.
    palette = Image.fromarray(np.array([[0, 1], [1, 0]], np.uint8), "P")
    palette.putpalette([255, 0, 0, 0, 0, 255])
    palette.info["transparency"] = 1
    planes = ffpsd.from_pil(palette)
    assert planes.shape == (4, 2, 2)
    assert planes[:, 0, 0].tolist() == [255, 0, 0, 255]
    assert planes[:, 0, 1].tolist() == [0, 0, 255, 0]

    # Gray by Pillow's own weights, without transparency for an opaque picture.
    rgb = Image.fromarray(interleaved(RGBA_PLANES[:3]))
    gray = ffpsd.from_pil(rgb, ffpsd.ColorMode.GRAYSCALE)
    assert np.array_equal(gray, np.asarray(rgb.convert("L"))[None])


def test_sixteen_bits_stay_for_gray_and_scale_for_colors():
    wide = np.array([[0, 1000], [40000, 65535]], np.uint16)
    planes = ffpsd.from_pil(Image.fromarray(wide), ffpsd.ColorMode.GRAYSCALE, 16)
    assert planes.dtype == np.uint16 and np.array_equal(planes[0], wide)
    back = ffpsd.to_pil(planes)
    assert back.mode == "I;16" and np.array_equal(np.asarray(back), wide)

    colors = ffpsd.from_pil(Image.fromarray(interleaved(RGBA_PLANES)), depth=16)
    assert np.array_equal(colors, RGBA_PLANES.astype(np.uint16) * 257)
    assert np.array_equal(np.asarray(ffpsd.to_pil(colors)), interleaved(RGBA_PLANES))


def test_a_pillow_picture_becomes_a_layer_and_comes_back():
    image = Image.fromarray(interleaved(RGBA_PLANES))
    doc = ffpsd.Document(image.width, image.height)
    doc.add_layer("picture", ffpsd.from_pil(image))
    read = ffpsd.Document.from_bytes(doc.to_bytes())
    assert np.array_equal(np.asarray(ffpsd.to_pil(read.layer(0).pixels())), np.asarray(image))


def test_what_pillow_and_ffpsd_do_not_share_is_an_error():
    image = Image.fromarray(interleaved(RGBA_PLANES))
    with pytest.raises(ffpsd.InvalidArgumentError):
        ffpsd.from_pil(image, ffpsd.ColorMode.CMYK)
    with pytest.raises(ffpsd.InvalidArgumentError):
        ffpsd.from_pil(image, depth=32)
    with pytest.raises(ValueError):
        ffpsd.to_pil(np.zeros((5, 2, 2), np.uint8))
    with pytest.raises(ValueError):
        ffpsd.to_pil(np.zeros((3, 2, 2), np.float32))
