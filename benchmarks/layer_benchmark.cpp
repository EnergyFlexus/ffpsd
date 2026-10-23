#include "support/benchmark_support.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <vector>

using namespace ffpsd_benchmark;

namespace
{
    [[maybe_unused]] const bool kRegistered = [] {
        // The RGBA copy layer of the 1890 x 1417 file: four RLE channels decoded into planes.
        Add("Layer/GetPixels/rgba", [](benchmark::State& state) {
            const ffpsd::Layer& layer = *Opened(kRgbPsd).GetLayerByIndex(1);
            for (auto _ : state)
                benchmark::DoNotOptimize(layer.GetPixels());
            SetBytes(state, RgbaPixels().bytes.size());
        });
        Add("Layer/GetPixelsBytes/rgba", [](benchmark::State& state) {
            const ffpsd::Layer& layer = *Opened(kRgbPsd).GetLayerByIndex(1);
            std::vector<std::uint8_t> bytes(layer.GetPixelsInfo().GetSizeBytes());
            for (auto _ : state)
            {
                layer.GetPixelsBytes(bytes.data(), bytes.size());
                benchmark::DoNotOptimize(bytes.data());
            }
            SetBytes(state, bytes.size());
        });

        // Planes packed into RLE channels, or raw where RLE would not be smaller.
        Add("Layer/AddLayer/rgba", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.AddLayer("added", RgbaPixels()); });
            SetBytes(state, RgbaPixels().bytes.size());
        });
        Add("Layer/SetPixels/rgba", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.GetLayerByIndex(1)->SetPixels(RgbaPixels()); });
            SetBytes(state, RgbaPixels().bytes.size());
        });

        Add("Layer/Resize/bicubic", [](benchmark::State& state) {
            TimeEdit(
                state, kRgbPsd, [](ffpsd::Document& doc) { doc.GetLayerByIndex(1)->Resize(1000, 750, ffpsd::ResampleFilter::kBicubic); });
        });
        Add("Layer/Resize/nearest", [](benchmark::State& state) {
            TimeEdit(
                state, kRgbPsd, [](ffpsd::Document& doc) { doc.GetLayerByIndex(1)->Resize(1000, 750, ffpsd::ResampleFilter::kNearest); });
        });
        Add("Layer/Rotate/quarter", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.GetLayerByIndex(1)->Rotate(ffpsd::Rotation::k90); });
        });
        Add("Layer/Flip/horizontal", [](benchmark::State& state) {
            TimeEdit(state, kRgbPsd, [](ffpsd::Document& doc) { doc.GetLayerByIndex(1)->Flip(ffpsd::FlipDirection::kHorizontal); });
        });
        return true;
    }();
} // namespace
