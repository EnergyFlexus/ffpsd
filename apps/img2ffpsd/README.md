# img2ffpsd

`img2ffpsd-cli` on the command line, and `img2ffpsd-ui`, the same in a window, built with `FFPSD_WITH_IMGUI`.
`cli/` and `ui/` hold their `main.cpp`; finding the pictures and converting them is in `common/`.

```sh
img2ffpsd-cli <bottom> <top> [<layer>...] <output> [--auto|--color|--gray] [--jobs N] [--resize nearest|bicubic]
```

Pairs the PNG and JPEG files of the bottom and top folders by their path without
the extension into PSD files: `raw/01.jpg` goes with `scaled/01.png`. A JPEG is
turned upright by its EXIF orientation, as Photoshop opens it. The bottom one is
the locked background, the top one the layer above it. Each further folder adds
a layer above those, where it has the file. A layer is named by its folder:
`Layer 1` from the top one, `Layer 2` from the next. Layers lose their
transparency, and the upper one is the composite. Every picture is resized to the
top one, by nearest neighbour or with `--resize bicubic`.

With `--auto`, the default, a document takes its colors from the top picture, as it
takes its size: grayscale when that picture is gray, RGB otherwise, and the others
are converted to it. A picture is gray when its file is, a PNG of gray or gray with
alpha or a JPEG of one component, or when every pixel has the same red, green and
blue, as a gray scan saved in color has. `--color` makes every document RGB and
`--gray` every one grayscale. The window has the same choice under Colors.

A file in only the bottom or the top folder stops it, and so do two pictures of
one name in a folder, `01.png` and `01.jpg`; a file that only a further folder
has is skipped with a warning. Gaps in numbered names (`1, 2, 4`; `3-4` is two
pages) and a non-empty output folder wait for a key, Ctrl+C to stop. Needs
`FFPSD_WITH_PNG` and `FFPSD_WITH_JPEG`.
