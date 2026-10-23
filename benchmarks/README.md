# Benchmarks

Google Benchmark, built by the debug presets and by `bench-release`; only the latter
gives numbers worth reading. The top-level README shows how to run and compare them.

```
document_benchmark.cpp   document.hpp: opening, saving in each compression, the composite,
                         PSB, color modes, and Resize, ResizeCanvas, RotateCanvas, FlipCanvas
layer_benchmark.cpp      layer.hpp: pixels out and in, and Resize, Rotate, Flip of one layer
formats_benchmark.cpp    formats.hpp: encoding and loading PNG, JPEG and WebP, as built
support/                 benchmark_support.hpp: the data files, Add, TimeEdit, SetBytes
```

A benchmark is named `Area/Call/case`, such as `Document/Save/raw` or
`Formats/EncodeWebp/lossless`, so `--filter Document/`, `--filter /Resize/` or
`--filter lossless` picks a part. Each is registered with `Add`, which also sets
milliseconds as the unit.

What the call does is all that is timed: a benchmark that changes a document gets a
fresh copy each iteration through `TimeEdit`, and the copy is not counted. Where the
amount of data is clear, `SetBytes` reports a rate: the file for opening, the document
unpacked for saving, the pixels for layers and pictures.

The inputs are Photoshop's RGB file of 1890 x 1417 and its layers, so the codecs and the
filters see real pictures rather than noise. Paths come from `FFPSD_BENCHMARK_DATA_DIR`,
which `--against` points at this checkout: Git LFS fills the files only here.