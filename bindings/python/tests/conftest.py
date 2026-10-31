"""What the tests share: the files of tests/data, whose values come from independent dumps."""

from pathlib import Path
from typing import TypeVar

import numpy as np
import pytest

DATA = Path(__file__).resolve().parents[3] / "tests" / "data"

GRAYSCALE_PSD = DATA / "photoshop" / "grayscale_two_layers.psd"
RGB_PSD = DATA / "photoshop" / "rgb_two_layers.psd"
RGB_LEVELS_PSD = DATA / "photoshop" / "rgb_levels.psd"
RGB_MASKS_PSD = DATA / "photoshop" / "rgb_masks.psd"
RGBA_PNG = DATA / "generated" / "rgba_8bit.png"
RGBA_WEBP = DATA / "generated" / "rgba_lossless.webp"
QUADRANTS_JPEG = DATA / "generated" / "rgb_quadrants.jpg"

BACKGROUND_NAME = "Фон"
BACKGROUND_COPY_NAME = "Фон копия"
LEVELS_NAME = "Уровни 1"

# The planes of RGBA_PNG and RGBA_WEBP, 3 x 2: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
RGBA_PLANES = np.array(
    [
        [[255, 0, 0], [255, 0, 128]],  # R
        [[0, 255, 0], [255, 0, 128]],  # G
        [[0, 0, 255], [255, 0, 128]],  # B
        [[255, 128, 0], [255, 255, 64]],  # A
    ],
    dtype=np.uint8,
)


T = TypeVar("T")


def found(value: T | None) -> T:
    """What a lookup that has to succeed returned, typed without its None."""
    assert value is not None
    return value


def flat(width, height, values):
    """Planes of one value each, 8 bit."""
    return np.array([np.full((height, width), value, np.uint8) for value in values])


def plane_sums(pixels):
    return [int(plane.sum(dtype=np.uint64)) for plane in pixels]


@pytest.fixture
def temp(tmp_path):
    return lambda name: tmp_path / name
