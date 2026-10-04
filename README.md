# ffpsd

A PSD/PSB library with two APIs from one build:

* `ffpsd/ffpsd.hpp` - C++
* `ffpsd/c_api.h` - C, for other languages and toolchains

Code style: [CONTRIBUTING.md](CONTRIBUTING.md). Programs in C++, C and Rust:
[examples/](examples/README.md).

libpng, zlib-ng, libjpeg-turbo, Dear ImGui, SDL3, GoogleTest and Google Benchmark are git submodules under
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
`--no-apps`, `--no-png`, `--no-jpeg`, `--crt static|dynamic`, `--out DIR`.

```
ffpsd-out/bin/        ffpsd.dll, img2ffpsd, img2ffpsd_ui
ffpsd-out/lib/        ffpsd.lib / libffpsd.a, cmake/ffpsd/
                      static only: libpng16_static.lib, zlibstatic.lib, jpeg-static.lib
ffpsd-out/include/    ffpsd/*.hpp, ffpsd/c_api.h, ffpsd/export.h
ffpsd-out/share/      ffpsd/licenses/
```

A DLL has libpng, zlib-ng and libjpeg-turbo inside and exports none of their
symbols; a static library ships them next to it, and the CMake package links them.
libjpeg-turbo is plain C, without SIMD, so no assembler is needed.

## Apps

Built with `FFPSD_BUILD_APPS` and installed into `bin/`:

* [img2ffpsd](apps/img2ffpsd/README.md) - folders of PNG and JPEG pictures into layered PSD files
* [img2ffpsd_ui](apps/img2ffpsd_ui/) - the same in a window, with `FFPSD_WITH_IMGUI`; `build.py` builds it

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
| `FFPSD_BUILD_APPS`  | `ON`    | `apps/`                            |
| `FFPSD_WITH_PNG`    | `ON`    | PNG in `ffpsd/formats.hpp`, with libpng and zlib-ng |
| `FFPSD_WITH_JPEG`   | `ON`    | JPEG in `ffpsd/formats.hpp`, with libjpeg-turbo |
| `FFPSD_WITH_IMGUI`  | `OFF`   | the apps with a window, with Dear ImGui and SDL3 |
| `FFPSD_BUILD_TESTS` | `OFF`, `ON` in the debug presets | `tests/` |
| `FFPSD_BUILD_BENCHMARKS` | `OFF`, `ON` in `bench-release` and the debug presets | `benchmarks/` |
| `FFPSD_BUILD_EXAMPLES` | `OFF`, `ON` in the debug presets | `examples/` in C and C++ |
| `FFPSD_MSVC_STATIC_RUNTIME` | `ON` static, `OFF` shared | MSVC: `/MT` instead of `/MD` |

On top of a preset: `cmake --preset shared-release -DFFPSD_BUILD_APPS=OFF`.

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

## Trademarks

Adobe and Photoshop are either registered trademarks or trademarks of Adobe in
the United States and/or other countries. ffpsd is not affiliated with or
endorsed by Adobe.
