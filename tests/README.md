# Tests

GoogleTest, built by the debug presets; the top-level README shows how to run them.

A folder per public header, and a few for the rest. A new test goes where the
header it tests is, and its suite is named after the folder, so
`run_tests.py --filter Layer` runs `layer/`.

```
smoke/         Photoshop files read end to end, checked against an independent dump
document/      document.hpp: header, image resources, the stack, groups, saving
layer/         layer.hpp: properties, name, kind, blocks, pixels, background, resizing
adjustments/   adjustments.hpp: Levels
png/           png.hpp; only with FFPSD_WITH_PNG
c_api/         c_api.h alone; c_header_check.c compiles it as C
support/       test_support.hpp: the data files, their layer names, small builders
data/          photoshop/: written by Photoshop; generated/: written by scripts/
scripts/       what writes data/generated/; run by hand, never by the build
```

One behaviour per test, named as a sentence:
`DocumentTest.AStackChangeThatBreaksAGroupIsRolledBack`. A refusal is checked
together with what it leaves behind.

| File | What it holds |
|------|---------------|
| `photoshop/grayscale_two_layers.psd` | 836 x 1200 gray: a background and a fill layer with transparency |
| `photoshop/rgb_two_layers.psd` | 1890 x 1417 RGB, RLE: a background and its copy with transparency |
| `photoshop/rgb_levels.psd` | the same background under a Levels adjustment layer |
| `generated/rgba_8bit.png` | 3 x 2 RGBA with known colors and alpha |
| `generated/gray_16bit.png` | 2 x 2 16 bit gray |
| `generated/palette_transparent.png` | 2 x 1 palette, one entry fully transparent |

`photoshop/` is from Photoshop 2026, in Git LFS, and is never edited: it is what
the library is checked against. `generated/` is written by
`scripts/make_png_files.py` with zlib alone, so libpng is checked against
another encoder; run it again after changing it.
