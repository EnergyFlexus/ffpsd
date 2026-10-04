#!/usr/bin/env python3
"""Writes the JPEG test files into tests/data/generated/ with Pillow, so libjpeg-turbo is checked against another
encoder. Flat 16 x 8 blocks at quality 100 without chroma subsampling decode within a few levels of these colors,
which are the ones tests/formats/jpeg_test.cpp expects. The EXIF blocks are built by hand, to have both byte orders."""

import struct
from pathlib import Path

from PIL import Image

DATA = Path(__file__).resolve().parent.parent / 'data' / 'generated'

RED, GREEN, BLUE, WHITE = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)


def quadrants() -> Image.Image:
    """32 x 16: red top left, green top right, blue bottom left, white bottom right."""
    image = Image.new('RGB', (32, 16))
    for (left, top), color in (((0, 0), RED), ((16, 0), GREEN), ((0, 8), BLUE), ((16, 8), WHITE)):
        image.paste(color, (left, top, left + 16, top + 8))
    return image


def exif(orientation: int, little: bool = True) -> bytes:
    """APP1 payload with IFD0 holding the Orientation tag alone."""
    order = '<' if little else '>'
    tiff = (b'II' if little else b'MM') + struct.pack(order + 'HI', 42, 8)
    tiff += struct.pack(order + 'H', 1) + struct.pack(order + 'HHIHH', 0x0112, 3, 1, orientation, 0) + struct.pack(order + 'I', 0)
    return b'Exif\x00\x00' + tiff


def save(image: Image.Image, name: str, **options) -> None:
    image.save(DATA / name, 'JPEG', quality=100, subsampling=0, **options)


save(quadrants(), 'rgb_quadrants.jpg')
save(quadrants(), 'rgb_quadrants_progressive.jpg', progressive=True)

for orientation in range(1, 9):
    save(quadrants(), f'orientation_{orientation}.jpg', exif=exif(orientation))
save(quadrants(), 'orientation_6_big_endian.jpg', exif=exif(6, little=False))
# IFD0 says it has 5 entries, the data ends after the first one's tag.
save(quadrants(), 'orientation_broken.jpg', exif=exif(6)[:6 + 8] + struct.pack('<HH', 5, 0x0112))

# 16 x 8 gray: 64 on the left half, 192 on the right.
gray = Image.new('L', (16, 8), 64)
gray.paste(192, (8, 0, 16, 8))
save(gray, 'gray.jpg')

# 16 x 8 CMYK, which Pillow writes inverted with an Adobe marker, as Photoshop does: cyan left, half black right.
cmyk = Image.new('CMYK', (16, 8), (255, 0, 0, 0))
cmyk.paste((0, 0, 0, 128), (8, 0, 16, 8))
save(cmyk, 'cmyk.jpg')
