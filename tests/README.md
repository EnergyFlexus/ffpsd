# Tests

GoogleTest, built by the debug presets; the top-level README shows how to run them.

```
smoke/     Photoshop files read end to end, checked against an independent dump
document/  header, image resources, the layer stack, group nesting, saving
layer/     properties, name, kind, blocks, pixels, background, position, resizing, Levels
png/       loading and saving PNG; only with FFPSD_WITH_PNG
c_api/     the C API alone; c_header_check.c compiles c_api.h as C
support/   test_support.hpp: the data files, their layer names, small builders
data/      the files the tests read
scripts/   what made some of them; run by hand, never by the build
```

One behaviour per test, named as a sentence:
`DocumentTest.AStackChangeThatBreaksAGroupIsRolledBack`. A refusal is checked
together with what it leaves behind.

| File | What it holds |
|------|---------------|
| `grayscale_two_layers.psd` | 836 x 1200 gray: a background and a fill layer with transparency |
| `rgb_two_layers.psd` | 1890 x 1417 RGB, RLE: a background and its copy with transparency |
| `rgb_levels.psd` | the same background under a Levels adjustment layer |
| `rgba_8bit.png` | 3 x 2 RGBA with known colors and alpha |
| `gray_16bit.png` | 2 x 2 16 bit gray |
| `palette_transparent.png` | 2 x 1 palette, one entry fully transparent |

The PSD files are from Photoshop 2026, in Git LFS. The PNG files are written by
`scripts/make_png_files.py` with zlib alone, so libpng is checked against
another encoder; run it again after changing it.
