"""Opens a PSD, prints what it holds, copies the top layer, adds a gradient and saves the result."""

import sys

import ffpsd
import numpy as np


def run(input_path, output_path):
    doc = ffpsd.Document.open(input_path)

    # Layers go from the bottom up.
    print(f"{doc.width} x {doc.height}, {doc.depth} bit, {doc.layer_count} layers")
    for index, layer in enumerate(doc.layers):
        bounds = layer.bounds
        pixels = layer.pixels()

        # Pixels are planar, (planes, height, width), so the first plane is pixels[0].
        mean = f", mean of the first plane {int(pixels[0].mean())}" if pixels.dtype == np.uint8 and pixels.size else ""
        print(f"  {index}: {layer.name}, {bounds.width} x {bounds.height}, {len(pixels)} planes{mean}")

    # Put a copy of the top layer on top; a group marker alone cannot be copied.
    doc.duplicate_layer(-1)

    # Add a gradient over the canvas: red grows to the right, green down, blue stays at half.
    gray = doc.color_mode == ffpsd.ColorMode.GRAYSCALE
    columns = np.arange(doc.width) * 255 // doc.width
    rows = np.arange(doc.height) * 255 // doc.height
    red = np.broadcast_to(columns, (doc.height, doc.width))
    green = np.broadcast_to(rows[:, None], (doc.height, doc.width))
    blue = np.full((doc.height, doc.width), 128)
    planes = [red] if gray else [red, green, blue]
    doc.add_layer("Gradient", np.array(planes, dtype=np.uint8))

    # Save, each channel RLE or raw, whichever is smaller.
    doc.save(output_path, ffpsd.Compression.RLE_OR_RAW)
    print(f"saved {doc.layer_count} layers to {output_path}")


def main():
    if len(sys.argv) != 3:
        print("usage: python examples/python/main.py <in.psd> <out.psd>", file=sys.stderr)
        return 2
    try:
        run(sys.argv[1], sys.argv[2])
    except ffpsd.Error as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
