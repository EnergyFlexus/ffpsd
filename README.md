# ffpsd

A PSD/PSB library with two APIs from one build:

* `ffpsd/ffpsd.hpp` - C++
* `ffpsd/c_api.h` - C, for other languages and toolchains

Code style: [CONTRIBUTING.md](CONTRIBUTING.md). Programs in C++, C and Rust:
[examples/](examples/README.md).

libpng, zlib-ng, GoogleTest and Google Benchmark are git submodules under
`vendor/`; the test PSD files are in Git LFS (`git lfs install` before cloning).

```sh
git clone --recursive <url>
git submodule update --init --recursive   # in a clone made without them
git lfs pull                              # in a clone made without Git LFS
```

## Quick build

Release, installed into `ffpsd-out/`:

```sh
python build.py            # shared library
python build.py --static   # static library
```

It checks for CMake >= 3.23, Ninja and a compiler, sets up MSVC on Windows by
itself and fetches missing submodules. Flags: `--clean`, `--jobs N`,
`--no-tools`, `--no-png`, `--crt static|dynamic`, `--out DIR`.

```
ffpsd-out/bin/        ffpsd.dll, img2ffpsd
ffpsd-out/lib/        ffpsd.lib / libffpsd.a, cmake/ffpsd/
                      static only: libpng16_static.lib, zlibstatic.lib
ffpsd-out/include/    ffpsd/*.hpp, ffpsd/c_api.h, ffpsd/export.h
ffpsd-out/share/      ffpsd/licenses/
```

A DLL has libpng and zlib-ng inside and exports none of their symbols; a static
library ships them next to it, and the CMake package links them.

## img2ffpsd

```sh
img2ffpsd <bottom> <top> [<layer>...] <output> [--gray] [--jobs N] [--resize nearest|bicubic]
```

Pairs the PNG files of the bottom and top folders by their path into PSD files:
the bottom one as the locked background, the top one as the layer above it. Each
further folder adds a layer above those, where it has the file. A layer is named
by its folder: `Layer 1` from the top one, `Layer 2` from the next. Layers lose
their transparency, and the upper one is the composite. Every picture is resized
to the larger one of the bottom and the top, by nearest neighbour or with
`--resize bicubic`. RGB, or grayscale with `--gray`.

A file in only the bottom or the top folder stops it; one that only a further
folder has is skipped with a warning. Gaps in numbered names (`1, 2, 4`; `3-4` is
two pages) and a non-empty output folder wait for a key, Ctrl+C to stop. Needs
`FFPSD_WITH_PNG`.

## Build with CMake

CMake >= 3.23 and Ninja; on Windows, from an *x64 Native Tools Command Prompt*.

```sh
cmake --preset shared-release
cmake --build --preset shared-release
cmake --install build/shared-release     # into ffpsd-out/ unless --prefix says otherwise
```

| Preset           | Library | Build type |
|------------------|---------|------------|
| `static-debug`   | static  | Debug      |
| `static-release` | static  | Release    |
| `shared-debug`   | shared  | Debug      |
| `shared-release` | shared  | Release    |
| `bench-release`  | static  | Release, benchmarks only |

## Tests

The debug presets build them; the script builds and runs them from any shell:

```sh
python scripts/run_tests.py                     # static-debug
python scripts/run_tests.py --shared            # shared-debug
python scripts/run_tests.py --all               # both
python scripts/run_tests.py --filter ReadPsd    # by name
python scripts/run_tests.py --no-build          # just run
```

By hand: `cmake --preset static-debug`, `cmake --build --preset static-debug`,
`ctest --preset static-debug`.

## Benchmarks

Reading and writing the files in `tests/data/photoshop/`. The debug presets only build
them; numbers come from `bench-release`:

```sh
python scripts/run_benchmarks.py                    # everything, 10 repetitions
python scripts/run_benchmarks.py --filter rgb       # by regular expression
python scripts/run_benchmarks.py --save before      # keep this run as "before"
python scripts/run_benchmarks.py --compare before   # run again and compare
python scripts/run_benchmarks.py --no-build         # just run
```

Runs are kept in `build/bench-release/results/`. `--compare` uses Google
Benchmark's `compare.py`, which needs
`pip install -r vendor/benchmark/tools/requirements.txt`.

## Compiler flags

* Release: `/O2 /Ob3 /Oi /Ot /Gy` (MSVC) or `-O3`, and LTO where available.
* Debug: `/W4 /permissive-` (MSVC) or
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, `-Wold-style-cast` for C++.

## Options

| Option              | Default | Meaning                            |
|---------------------|---------|------------------------------------|
| `BUILD_SHARED_LIBS` | `OFF`   | shared instead of static           |
| `FFPSD_BUILD_TOOLS` | `ON`    | `img2ffpsd`                        |
| `FFPSD_WITH_PNG`    | `ON`    | `ffpsd/png.hpp`, with libpng and zlib-ng |
| `FFPSD_BUILD_TESTS` | `OFF`, `ON` in the debug presets | `tests/` |
| `FFPSD_BUILD_BENCHMARKS` | `OFF`, `ON` in `bench-release` and the debug presets | `benchmarks/` |
| `FFPSD_BUILD_EXAMPLES` | `OFF`, `ON` in the debug presets | `examples/` in C and C++ |
| `FFPSD_MSVC_STATIC_RUNTIME` | `ON` static, `OFF` shared | MSVC: `/MT` instead of `/MD` |

On top of a preset: `cmake --preset shared-release -DFFPSD_BUILD_TOOLS=OFF`.

## Use from another CMake project

```cmake
find_package(ffpsd 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE ffpsd::ffpsd)
```

The package defines `FFPSD_STATIC` for a static build, so `ffpsd/export.h`
picks the right linkage. On MSVC the CRT follows the library kind:

* **Static library:** `/MT`, `/MTd`; the program needs the same, or it links two
  CRTs:

  ```sh
  # quoted: the shell would read < > as redirection
  cmake -S . -B build '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
  ```

* **DLL:** `/MD`, `/MDd`. The C++ API passes `std::string` and `std::vector`, so
  the program needs `/MD` too, the same compiler and the same configuration.

A DLL built with `-DFFPSD_MSVC_STATIC_RUNTIME=ON` has its own CRT and heap, so
only the C API is safe with it; CMake warns about it.
