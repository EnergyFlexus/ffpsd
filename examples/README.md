# Examples

One program in four languages, all of it in one function, `run`: it opens a PSD, prints its size, the size
of its composite as PNG and its top layer, puts a copy of that layer on top in Multiply mode, adds a PNG,
JPEG or WebP as a layer, resized, mirrored and with a mask, adds a Levels layer, grows the canvas and saves
the result. The four write the same file.

```sh
ffpsd_example_cpp tests/data/photoshop/rgb_two_layers.psd tests/data/generated/rgb_quadrants.jpg out.psd
```

```
1890 x 1417, 8 bit, 2 layers
composite: 59415 bytes of PNG
top layer: <name>, 1890 x 1417
saved 5 layers to out.psd
```

| Folder | API | Build |
|--------|-----|-------|
| `cpp/` | C++, `ffpsd/ffpsd.hpp` | CMake |
| `c/`   | C, `ffpsd/c_api.h` | CMake |
| `rust/` | Rust, the `ffpsd` crate | Cargo |
| `python/` | Python, the `ffpsd` package | pip |

## C and C++

The debug presets build them into `build/<preset>/bin/`; elsewhere turn on
`FFPSD_BUILD_EXAMPLES`. Each folder also builds on its own against an installed
ffpsd, and on Windows copies the shared library next to the program:

```sh
python build.py
cmake -S examples/cpp -B build/example-cpp -DCMAKE_PREFIX_PATH=ffpsd-out
cmake --build build/example-cpp
```

## Rust

A member of the Cargo workspace at the root; `ffpsd-sys` builds ffpsd for it:

```sh
cargo run -p ffpsd-example -- tests/data/photoshop/rgb_two_layers.psd tests/data/generated/rgb_quadrants.jpg out.psd
```

## Python

`pip install .` at the root builds the package from this checkout; `pip install ffpsd` takes the released one:

```sh
pip install .
python examples/python/main.py tests/data/photoshop/rgb_two_layers.psd tests/data/generated/rgb_quadrants.jpg out.psd
```
