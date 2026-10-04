# Tests

GoogleTest, built by the debug presets; the top-level README shows how to run them.

A folder per public header, and a file per part of it. A suite is the folder
and the file, `DocumentStackTest` for `document/stack_test.cpp`, just the folder
for the folder's main file, so `run_tests.py --filter Document` runs `document/`.

```
smoke/         Photoshop files end to end against scripts/dump_psd.py, read and written
document/      document.hpp: the header, resources, the stack, parsing, saving,
               PSD and PSB, compression
layer/         layer.hpp: properties, name, kind and blocks, pixels and position,
               the background, resizing
adjustments/   adjustments.hpp: Levels
formats/       formats.hpp: LoadPicture; png and jpeg only with their options
c_api/         c_api.h alone; c_header_check.c compiles it as C
support/       test_support.hpp: the data files, their layer names, small builders
data/          photoshop/: written by Photoshop; generated/: written by scripts/
scripts/       what writes data/generated/ and dumps data/photoshop/; run by hand
```

One feature per test, named as a sentence:
`DocumentStackTest.AStackChangeThatBreaksAGroupIsRolledBack`. Its cases go into the
same test, after a one-line comment where they need one, and variations are a loop
rather than parameters. No two tests check the same thing. A refusal is checked
together with what it leaves behind.

`python scripts/run_coverage.py` builds them with clang's coverage into
`build/coverage/` and reports what of `ffpsd/src` and `ffpsd/include` they reach;
`--save before.json`, then `--compare before.json` after a change, lists every line
and branch that is no longer covered. `--html DIR` shows it line by line.

| File | What it holds |
|------|---------------|
| `photoshop/grayscale_two_layers.psd` | 836 x 882 gray: a background and its copy with transparency |
| `photoshop/rgb_two_layers.psd` | 1890 x 1417 RGB, RLE: a background and its copy with transparency |
| `photoshop/rgb_levels.psd` | the same background under a Levels adjustment layer |
| `photoshop/grayscale_two_layers_levels.psd` | 836 x 879 gray: a background, its copy, whose 'lnsr' says 'bgnd' too, and a Levels layer of 25 to 237 |
| `photoshop/rgb_masks.psd` | 1890 x 1417 RGB: a layer mask over the right half of a layer, and a pixel mask with an empty vector mask, channels -2 and -3 |
| `generated/rgba_8bit.png` | 3 x 2 RGBA with known colors and alpha |
| `generated/gray_16bit.png` | 2 x 2 16 bit gray |
| `generated/palette_transparent.png` | 2 x 1 palette, one entry fully transparent |
| `generated/rgb_quadrants.jpg` | 32 x 16 RGB: red, green, blue and white quarters |
| `generated/rgb_quadrants_progressive.jpg` | the same, progressive |
| `generated/orientation_<1-8>.jpg` | the same pixels with each EXIF orientation |
| `generated/orientation_6_big_endian.jpg` | orientation 6 in big endian EXIF |
| `generated/orientation_broken.jpg` | EXIF that ends inside its first entry |
| `generated/gray.jpg` | 16 x 8 gray: 64 on the left, 192 on the right |
| `generated/cmyk.jpg` | 16 x 8 Adobe CMYK: cyan on the left, half black on the right |

`photoshop/` is from Photoshop 2026, in Git LFS, and is never edited: it is what
the library is checked against. `generated/` is written by
`scripts/make_png_files.py` with zlib alone, so libpng is checked against
another encoder, and by `scripts/make_jpeg_files.py` with Pillow
(`pip install -r scripts/requirements.txt`); run them again after changing them.
