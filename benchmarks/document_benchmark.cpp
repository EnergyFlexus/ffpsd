#include "support/benchmark_support.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <string>
#include <vector>

using namespace ffpsd_benchmark;

namespace
{
    // The whole file into a Document: disk, header, resources, layers and channel data.
    void Open(benchmark::State& state, const std::string& path)
    {
        for (auto _ : state)
            benchmark::DoNotOptimize(ffpsd::Document::Open(path));
        SetBytes(state, std::filesystem::file_size(path));
    }

    // Back to bytes in memory, the disk left out; the rate is of the document unpacked, whatever comes out.
    void Save(benchmark::State& state, const ffpsd::Document& doc, ffpsd::Compression compression)
    {
        for (auto _ : state)
        {
            const std::vector<std::uint8_t> bytes = doc.Save(compression);
            benchmark::DoNotOptimize(bytes.data());
        }
        SetBytes(state, doc.Save(ffpsd::Compression::kRaw).size());
    }

    [[maybe_unused]] const bool kRegistered = [] {
        Add("Document/Open/gray", [](benchmark::State& state) { Open(state, kGrayscalePsd); });
        Add("Document/Open/rgb", [](benchmark::State& state) { Open(state, kRgbPsd); });
        Add("Document/Open/masks", [](benchmark::State& state) { Open(state, kRgbMasksPsd); });

        // Photoshop's RLE written as it is, raw, packed again, and raw packed into RLE where it is smaller.
        Add("Document/Save/as_stored", [](benchmark::State& state) { Save(state, Opened(kRgbPsd), ffpsd::Compression::kDefault); });
        Add("Document/Save/raw", [](benchmark::State& state) { Save(state, Opened(kRgbPsd), ffpsd::Compression::kRaw); });
        Add("Document/Save/rle", [](benchmark::State& state) { Save(state, Opened(kRgbPsd), ffpsd::Compression::kRle); });
        Add("Document/Save/raw_to_rle_or_raw", [](benchmark::State& state) {
            const ffpsd::Document raw = ffpsd::Document::Parse(Opened(kRgbPsd).Save(ffpsd::Compression::kRaw));
            Save(state, raw, ffpsd::Compression::kRleOrRaw);
        });

        Add("Document/GetMergedImage/rgb", [](benchmark::State& state) {
            const ffpsd::Document& doc = Opened(kRgbPsd);
            for (auto _ : state)
                benchmark::DoNotOptimize(doc.GetMergedImage());
            SetBytes(state, doc.GetMergedImageInfo().GetSizeBytes());
        });
        Add("Document/SetPsb/rgb",
            [](benchmark::State& state) { TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.SetPsb(true); }); });
        Add("Document/ConvertColorMode/rgb_to_gray", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.ConvertColorMode(ffpsd::ColorMode::kGrayscale); });
        });

        // Every layer and the composite of the 1890 x 1417 file.
        Add("Document/Resize/bicubic", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.Resize(1200, 900, ffpsd::ResampleFilter::kBicubic); });
        });
        Add("Document/Resize/nearest", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.Resize(1200, 900, ffpsd::ResampleFilter::kNearest); });
        });
        Add("Document/ResizeCanvas/grow",
            [](benchmark::State& state) { TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.ResizeCanvas(2000, 1500); }); });
        Add("Document/RotateCanvas/quarter", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.RotateCanvas(ffpsd::Rotation::k90); });
        });
        Add("Document/FlipCanvas/horizontal", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.FlipCanvas(ffpsd::FlipDirection::kHorizontal); });
        });
        return true;
    }();
} // namespace
