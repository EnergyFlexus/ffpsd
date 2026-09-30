# img2ffpsd

```sh
img2ffpsd <bottom> <top> [<layer>...] <output> [--gray] [--jobs N] [--resize nearest|bicubic]
```

Pairs the PNG and JPEG files of the bottom and top folders by their path without
the extension into PSD files: `raw/01.jpg` goes with `scaled/01.png`. A JPEG is
turned upright by its EXIF orientation, as Photoshop opens it. The bottom one is
the locked background, the top one the layer above it. Each further folder adds
a layer above those, where it has the file. A layer is named by its folder:
`Layer 1` from the top one, `Layer 2` from the next. Layers lose their
transparency, and the upper one is the composite. Every picture is resized to the
top one, by nearest neighbour or with `--resize bicubic`. RGB, or grayscale with
`--gray`.

A file in only the bottom or the top folder stops it, and so do two pictures of
one name in a folder, `01.png` and `01.jpg`; a file that only a further folder
has is skipped with a warning. Gaps in numbered names (`1, 2, 4`; `3-4` is two
pages) and a non-empty output folder wait for a key, Ctrl+C to stop. Needs
`FFPSD_WITH_PNG` and `FFPSD_WITH_JPEG`.
