# Examples

One program in three languages: it opens a PSD, prints its size, color mode,
resolution, writer and layers, puts a copy of the top layer on top and saves.

```sh
ffpsd_example_cpp tests/data/photoshop/rgb_levels.psd out.psd
```

```
1890 x 1417, RGB, 8 bit
resolution: 300 x 300 ppi
written by: Adobe Photoshop, for Adobe Photoshop 2026
layers, bottom to top: 2
  0: <name> (raster), 1890 x 1417 at 0, 0
  1: <name> (adjustment), 0 x 0 at 0, 0

copied <name> to the top, saved 3 layers to out.psd
```

| Folder | API | Build |
|--------|-----|-------|
| `cpp/` | C++, `ffpsd/ffpsd.hpp` | CMake |
| `c/`   | C, `ffpsd/c_api.h` | CMake |
| `rust/` | C, bindings by hand in `src/ffi.rs` | Cargo |

## C and C++

The debug presets build them into `build/<preset>/bin/`; elsewhere turn on
`FFPSD_BUILD_EXAMPLES`. Each folder also builds on its own against an installed
ffpsd, and copies a DLL next to the program:

```sh
python build.py
cmake -S examples/cpp -B build/example-cpp -DCMAKE_PREFIX_PATH=ffpsd-out
cmake --build build/example-cpp
```

## Rust

Links a shared ffpsd from `ffpsd-out/`, or from `FFPSD_DIR`:

```sh
python build.py
cd examples/rust
cargo run -- ../../tests/data/photoshop/rgb_levels.psd out.psd
```

On Windows `ffpsd-out/bin` must be on the `PATH`; on Linux and macOS the program
records where the library is. Paths reach ffpsd in the system code page on
Windows, so characters outside it do not open.
