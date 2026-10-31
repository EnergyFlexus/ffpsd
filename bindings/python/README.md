# ffpsd

Python bindings to [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes Photoshop PSD
and PSB files.

```python
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
```

## What it does for now

* PSD and PSB, and either into the other; RGB and gray documents, and either into the other.
* Layers: raster layers, groups, the background and Levels adjustments; added, copied from this or another
  document, moved and removed; name, opacity, visibility, clipping and blend mode; pixels in and out.
* Pixel masks, read, set and removed.
* The canvas and the layers resized, turned and flipped; the canvas grown or cut from any anchor.
* The composite the file stores, read and replaced; resolution and version info.
* Every image resource and tagged block, raw, for what ffpsd does not interpret itself.
* PNG, JPEG and WebP in and out.

## In Python

* Pixels are NumPy arrays, planar as PSD keeps them: `(channels, height, width)`, uint8, uint16 or float32 by
  the depth. Arrays ffpsd gives own their memory without a copy; one it takes is copied only when it is not
  C-contiguous, so `pixels.transpose(2, 0, 1)` of an interleaved picture works as is.
* Calls that take time, such as saving, pixels in and out and resizing, release the GIL, so separate documents
  work in threads at once. A document belongs to one thread at a time: touching one that another thread is
  working on raises `ffpsd.InvalidOperationError`.
* Errors are `ffpsd.Error` and also the built-in exception they mean: `ValueError`, `IndexError`, `OSError`,
  `RuntimeError` or `NotImplementedError`.
* A layer removed from its document raises on use instead of crashing.
* With `pip install ffpsd[pil]`, `ffpsd.from_pil` and `ffpsd.to_pil` take and give Pillow images.
* Type stubs come with the package.

More in the [repository](https://github.com/EnergyFlexus/ffpsd): building from source and the
[roadmap](https://github.com/EnergyFlexus/ffpsd/blob/main/ROADMAP.md).

## License

MIT. The vendored libraries built in keep their own permissive licenses, which the package carries.
