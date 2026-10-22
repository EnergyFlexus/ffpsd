#!/usr/bin/env python3
"""Writes the WebP test files into tests/data/generated/ with Pillow, so the files are not ffpsd's own. The lossless one
holds the pixels of rgba_8bit.png exactly, the colors under its clear pixel too; the lossy one has the flat quarters of
rgb_quadrants.jpg, which decode within a few levels of their colors. Both are what tests/formats/webp_test.cpp expects."""

from pathlib import Path

from PIL import Image

DATA = Path(__file__).resolve().parent.parent / 'data' / 'generated'

RED, GREEN, BLUE, WHITE = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)

# 3 x 2, as in rgba_8bit.png: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
rgba = Image.new('RGBA', (3, 2))
rgba.putdata([(255, 0, 0, 255), (0, 255, 0, 128), (0, 0, 255, 0), (255, 255, 255, 255), (0, 0, 0, 255), (128, 128, 128, 64)])
rgba.save(DATA / 'rgba_lossless.webp', 'WEBP', lossless=True, exact=True)

# 32 x 16: red top left, green top right, blue bottom left, white bottom right.
quadrants = Image.new('RGB', (32, 16))
for (left, top), color in (((0, 0), RED), ((16, 0), GREEN), ((0, 8), BLUE), ((16, 8), WHITE)):
    quadrants.paste(color, (left, top, left + 16, top + 8))
quadrants.save(DATA / 'rgb_quadrants.webp', 'WEBP', quality=100)

# Two 4 x 4 frames, red then blue, which loading refuses.
frames = [Image.new('RGB', (4, 4), RED), Image.new('RGB', (4, 4), BLUE)]
frames[0].save(DATA / 'animated.webp', 'WEBP', save_all=True, append_images=frames[1:], duration=100, lossless=True)
