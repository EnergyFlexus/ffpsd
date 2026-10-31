"""Opens a PSD, adds a copy of the top layer, a picture with a mask and a Levels layer, grows the canvas and saves the result."""

import io
import sys

import ffpsd
import numpy as np


def run(input_path, picture_path, output_path):
    doc = ffpsd.Document.open(input_path)
    width, height = doc.width, doc.height
    print(f"{width} x {height}, {doc.depth} bit, {doc.layer_count} layers")

    # The composite Photoshop saved with the file, as PNG; None when there is none.
    composite = doc.merged_image()
    if composite is not None:
        print(f"composite: {len(ffpsd.encode_png(composite))} bytes of PNG")

    # Layers go from the bottom up, so the top one is -1.
    top = doc.layer(-1)
    print(f"top layer: {top.name}, {top.bounds.width} x {top.bounds.height}")

    # A copy of it on top, half transparent, in Multiply mode.
    copy = doc.duplicate_layer(-1)
    copy.name = "Copy"
    copy.opacity = 128
    copy.blend_key = ffpsd.fourcc("mul ")

    # A PNG, JPEG or WebP at (50, 50), half the canvas, mirrored.
    image = ffpsd.load_picture(picture_path, doc.color_mode, doc.depth)
    picture = doc.add_layer("Picture", image, 50, 50)
    picture.resize(width // 2, height // 2)
    picture.flip(ffpsd.FlipDirection.HORIZONTAL)

    # A mask that hides a 100 x 100 square in its corner.
    picture.set_mask(np.zeros((100, 100), np.uint8), 50, 50, 255)

    # A Levels layer over everything that brightens the midtones.
    levels = ffpsd.Levels(channels=[ffpsd.LevelsChannel(gamma=1.5)])
    doc.add_levels_layer("Levels", levels)

    # 100 px more canvas on every side.
    doc.resize_canvas(width + 200, height + 200, ffpsd.Anchor.CENTER)
    # Each channel RLE or raw, whichever is smaller.
    doc.save(output_path, ffpsd.Compression.RLE_OR_RAW)
    print(f"saved {doc.layer_count} layers to {output_path}")


def main():
    # Layer names are UTF-8; on Windows a redirected stdout would otherwise take the system code page, cp1252 say.
    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding="utf-8")
    if len(sys.argv) != 4:
        print("usage: python examples/python/main.py <in.psd> <picture> <out.psd>", file=sys.stderr)
        return 2
    try:
        run(sys.argv[1], sys.argv[2], sys.argv[3])
    except ffpsd.Error as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
