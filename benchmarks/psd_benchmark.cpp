#include <benchmark/benchmark.h>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    std::string TestFile(const char* name)
    {
        return std::string(FFPSD_TEST_DATA_DIR) + "/" + name;
    }

    // The whole file into a Document: disk, header, resources, layers and channel data.
    void Read(benchmark::State& state, const char* name)
    {
        const std::string path = TestFile(name);
        for (auto _ : state)
        {
            ffpsd::Document doc = ffpsd::Document::Parse(path);
            benchmark::DoNotOptimize(doc);
        }
        state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(std::filesystem::file_size(path)));
    }

    // A parsed Document back to bytes in memory; the disk stays out.
    void Write(benchmark::State& state, const char* name)
    {
        const ffpsd::Document doc = ffpsd::Document::Parse(TestFile(name));
        std::int64_t size = 0;
        for (auto _ : state)
        {
            std::vector<std::uint8_t> data = doc.Save();
            size = static_cast<std::int64_t>(data.size());
            benchmark::DoNotOptimize(data);
        }
        state.SetBytesProcessed(state.iterations() * size);
    }
} // namespace

BENCHMARK_CAPTURE(Read, grayscale, "photoshop/grayscale_two_layers.psd")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(Read, rgb, "photoshop/rgb_two_layers.psd")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(Write, grayscale, "photoshop/grayscale_two_layers.psd")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(Write, rgb, "photoshop/rgb_two_layers.psd")->Unit(benchmark::kMillisecond);
