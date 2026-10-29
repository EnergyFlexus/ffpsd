# ffpsd-sys

Raw bindings to the C API of [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes
Photoshop PSD and PSB files. The [`ffpsd`](https://crates.io/crates/ffpsd) crate wraps them.

ffpsd is linked from the first of:

1. `FFPSD_DIR=<folder>`: an installed ffpsd, static or shared.
2. `FFPSD_FROM_SOURCE=1`: a build from the sources in this crate, with CMake.
3. The static library of the GitHub release for the target, or the archive `FFPSD_ARCHIVE` names.
4. A build from the sources in this crate.

A build from source needs CMake 3.23 or later and a C++17 compiler.

## License

MIT
