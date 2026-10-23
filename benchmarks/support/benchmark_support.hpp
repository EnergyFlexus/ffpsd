#ifndef FFPSD_BENCHMARKS_SUPPORT_BENCHMARK_SUPPORT_HPP_
#define FFPSD_BENCHMARKS_SUPPORT_BENCHMARK_SUPPORT_HPP_

#include <benchmark/benchmark.h>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <functional>
#include <map>
#include <string>

namespace ffpsd_benchmark
{
    // Photoshop files of the tests; the RGB one is 1890 x 1417, a background and a copy with transparency.
    inline const std::string kRgbPsd = std::string(FFPSD_BENCHMARK_DATA_DIR) + "/photoshop/rgb_two_layers.psd";
    inline const std::string kGrayscalePsd = std::string(FFPSD_BENCHMARK_DATA_DIR) + "/photoshop/grayscale_two_layers.psd";
    inline const std::string kRgbMasksPsd = std::string(FFPSD_BENCHMARK_DATA_DIR) + "/photoshop/rgb_masks.psd";

    // Read once and kept: a benchmark that changes a document takes a Fresh copy instead.
    inline const ffpsd::Document& Opened(const std::string& path)
    {
        static std::map<std::string, ffpsd::Document> documents;
        auto at = documents.find(path);
        if (at == documents.end())
            at = documents.emplace(path, ffpsd::Document::Open(path)).first;
        return at->second;
    }

    inline ffpsd::Document Fresh(const std::string& path)
    {
        return ffpsd::Document::Parse(Opened(path).Save());
    }

    // The RGBA copy layer of kRgbPsd, the pixels most benchmarks work on.
    inline const ffpsd::Image& RgbaPixels()
    {
        static const ffpsd::Image pixels = Opened(kRgbPsd).GetLayerByIndex(1)->GetPixels();
        return pixels;
    }

    // Times edit on a fresh copy of the file each iteration; making the copy is not timed.
    template <class Edit> void TimeEdit(benchmark::State& state, const std::string& path, const Edit& edit)
    {
        for (auto _ : state)
        {
            state.PauseTiming();
            ffpsd::Document doc = Fresh(path);
            state.ResumeTiming();
            edit(doc);
            benchmark::DoNotOptimize(doc);
        }
    }

    inline void SetBytes(benchmark::State& state, std::uintmax_t bytes)
    {
        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations() * bytes));
    }

    // Every benchmark is named Area/Call/case, so --filter picks an area, a call or a case.
    inline benchmark::Benchmark* Add(const std::string& name, std::function<void(benchmark::State&)> body)
    {
        return benchmark::RegisterBenchmark(name, [body](benchmark::State& state) { body(state); })->Unit(benchmark::kMillisecond);
    }
} // namespace ffpsd_benchmark

#endif // FFPSD_BENCHMARKS_SUPPORT_BENCHMARK_SUPPORT_HPP_
