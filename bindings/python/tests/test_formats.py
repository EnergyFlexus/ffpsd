"""PNG, JPEG and WebP in and out of the planes a layer takes."""

import ffpsd
import numpy as np
import pytest
from conftest import QUADRANTS_JPEG, RGBA_PLANES, RGBA_PNG, RGBA_WEBP


@pytest.mark.parametrize("format", [ffpsd.Format.PNG, ffpsd.Format.JPEG, ffpsd.Format.WEBP])
def test_every_format_is_in_the_default_build(format):
    assert ffpsd.is_format_supported(format)


def test_png_and_webp_load_known_pixels():
    assert np.array_equal(ffpsd.load_png(RGBA_PNG), RGBA_PLANES)
    assert np.array_equal(ffpsd.load_webp(RGBA_WEBP), RGBA_PLANES)
    assert np.array_equal(ffpsd.load_picture(RGBA_PNG), RGBA_PLANES)

    # As gray: 0.2126, 0.7152 and 0.0722 of 255, rounded, then the alpha.
    gray = ffpsd.load_png(RGBA_PNG, ffpsd.ColorMode.GRAYSCALE)
    assert gray.ravel().tolist() == [54, 182, 18, 255, 0, 128, 255, 128, 0, 255, 255, 64]


def test_lossless_formats_round_trip(temp):
    png = ffpsd.encode_png(RGBA_PLANES, ffpsd.PngCompression.SMALLEST)
    assert np.array_equal(ffpsd.decode_png(png), RGBA_PLANES)
    webp = ffpsd.encode_webp(RGBA_PLANES, 90, lossless=True)
    assert np.array_equal(ffpsd.decode_webp(webp), RGBA_PLANES)
    assert np.array_equal(ffpsd.decode_picture(webp), RGBA_PLANES)

    path = temp("rgba.png")
    ffpsd.save_png(RGBA_PLANES, path)
    assert np.array_equal(ffpsd.load_png(path), RGBA_PLANES)


def test_jpeg_keeps_the_shape_and_drops_alpha(temp):
    assert len(ffpsd.load_jpeg(QUADRANTS_JPEG, apply_orientation=True)) == 3

    jpeg = ffpsd.encode_jpeg(RGBA_PLANES, 95)
    assert ffpsd.decode_jpeg(jpeg, apply_orientation=False).shape == (3, 2, 3)

    path = temp("rgba.jpg")
    ffpsd.save_jpeg(RGBA_PLANES, path, 80)
    ffpsd.save_webp(RGBA_PLANES, temp("rgba.webp"), 80)
    assert len(ffpsd.load_picture(path)) == 3


def test_bad_input_is_an_error():
    with pytest.raises(ffpsd.InvalidFileError):
        ffpsd.decode_png(b"not a png")
    with pytest.raises(ffpsd.InvalidArgumentError):
        ffpsd.encode_jpeg(RGBA_PLANES, 0)
    with pytest.raises(ValueError):
        ffpsd.encode_png(RGBA_PLANES, color_mode=ffpsd.ColorMode.CMYK)


def test_layer_takes_a_loaded_picture():
    doc = ffpsd.Document(3, 2)
    layer = doc.add_layer("picture", ffpsd.load_png(RGBA_PNG))
    assert np.array_equal(layer.pixels(), RGBA_PLANES)
