"""Pillow images in and out of the planes ffpsd takes, for the formats ffpsd does not read or write itself."""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

from ._errors import InvalidArgumentError
from ._ffpsd import ColorMode

if TYPE_CHECKING:
    from numpy.typing import NDArray
    from PIL.Image import Image


def _pillow():
    try:
        from PIL import Image
    except ImportError as error:
        raise ImportError("ffpsd.from_pil and ffpsd.to_pil need Pillow: pip install ffpsd[pil]") from error
    return Image


def from_pil(image: Image, color_mode: ColorMode = ColorMode.RGB, depth: int = 8) -> NDArray:
    """The planes of a Pillow image, (channels, height, width), as a layer of that mode and depth takes them.

    Gray or RGB, 8 or 16 bit; transparency, when the image has it, is the last plane. Pillow keeps 16 bits
    only for gray, so colors at 16 bit are their 8 bits scaled up.
    """
    if color_mode not in (ColorMode.RGB, ColorMode.GRAYSCALE):
        raise InvalidArgumentError(f"ffpsd: a Pillow image goes into gray or RGB, not {color_mode.name}")
    if depth not in (8, 16):
        raise InvalidArgumentError(f"ffpsd: a Pillow image gives 8 or 16 bit, not {depth}")

    alpha = "A" in image.getbands() or "transparency" in image.info
    gray = color_mode == ColorMode.GRAYSCALE
    if gray and depth == 16 and image.mode in ("I;16", "I") and not alpha:
        planes = np.asarray(image.convert("I"), dtype=np.int64).clip(0, 65535).astype(np.uint16)[None]
        return np.ascontiguousarray(planes)

    mode = ("LA" if alpha else "L") if gray else ("RGBA" if alpha else "RGB")
    pixels = np.asarray(image.convert(mode))
    if pixels.ndim == 2:
        pixels = pixels[..., None]
    planes = pixels.transpose(2, 0, 1)
    if depth == 16:
        planes = planes.astype(np.uint16) * 257
    return np.ascontiguousarray(planes)


def to_pil(planes: NDArray, color_mode: ColorMode | None = None) -> Image:
    """A Pillow image of planes as ffpsd gives them, gray or RGB, with transparency as the last plane.

    Without a mode, one or two planes are gray and three or four RGB, as for encode_png. Gray at 16 bit stays
    16 bit; Pillow has no 16 bit color, so RGB comes down to 8.
    """
    Image = _pillow()
    planes = np.asarray(planes)
    if planes.ndim == 2:
        planes = planes[None]
    if planes.ndim != 3:
        raise InvalidArgumentError(f"ffpsd: planes are (channels, height, width), not {planes.ndim} dimensions")
    count = len(planes)
    if color_mode is None:
        color_mode = ColorMode.GRAYSCALE if count <= 2 else ColorMode.RGB
    colors = {ColorMode.GRAYSCALE: 1, ColorMode.RGB: 3}.get(color_mode)
    if colors is None or count not in (colors, colors + 1):
        raise InvalidArgumentError(f"ffpsd: {count} planes do not make a gray or RGB Pillow image")
    if planes.dtype not in (np.uint8, np.uint16):
        raise InvalidArgumentError(f"ffpsd: a Pillow image takes 8 or 16 bit planes, not {planes.dtype}")

    if planes.dtype == np.uint16 and count > 1:
        planes = (planes >> 8).astype(np.uint8)
    # Pillow takes the mode from the array: L, LA, RGB, RGBA, or I;16 for one 16 bit plane.
    pixels = planes[0] if count == 1 else planes.transpose(1, 2, 0)
    return Image.fromarray(np.ascontiguousarray(pixels))
