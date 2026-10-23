#include "support/benchmark_support.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <ffpsd/ffpsd.hpp>
#include <functional>
#include <string>
#include <vector>

using namespace ffpsd_benchmark;

namespace
{
    using Encode = std::function<std::vector<std::uint8_t>(const ffpsd::ImageView&)>;
    using Load = std::function<ffpsd::Image(const std::vector<std::uint8_t>&)>;

    // Real pixels rather than noise, so the codecs see what they would in a file: the copy layer, or the composite for RGB.
    const ffpsd::Image& Picture(std::uint16_t channels)
    {
        static const ffpsd::Image rgb = Opened(kRgbPsd).GetMergedImage();
        return channels == 4 ? RgbaPixels() : rgb;
    }

    void TimeEncode(benchmark::State& state, std::uint16_t channels, const Encode& encode)
    {
        const ffpsd::Image& picture = Picture(channels);
        for (auto _ : state)
            benchmark::DoNotOptimize(encode(picture));
        SetBytes(state, picture.bytes.size());
    }

    void TimeLoad(benchmark::State& state, std::uint16_t channels, const Encode& encode, const Load& load)
    {
        const ffpsd::Image& picture = Picture(channels);
        const std::vector<std::uint8_t> file = encode(picture);
        for (auto _ : state)
            benchmark::DoNotOptimize(load(file));
        SetBytes(state, picture.bytes.size());
    }

    // A codec's encode and load under Formats/Encode<name>/<case> and Formats/Load<name>/<case>.
    void AddCodec(const std::string& name, const std::string& which, std::uint16_t channels, const Encode& encode, const Load& load)
    {
        Add("Formats/Encode" + name + "/" + which, [=](benchmark::State& state) { TimeEncode(state, channels, encode); });
        Add("Formats/Load" + name + "/" + which, [=](benchmark::State& state) { TimeLoad(state, channels, encode, load); });
    }

    [[maybe_unused]] const bool kRegistered = [] {
#if defined(FFPSD_HAS_PNG)
        const Load png = [](const std::vector<std::uint8_t>& file) {
            return ffpsd::LoadPng(file.data(), file.size(), ffpsd::ColorMode::kRgb, 8);
        };
        AddCodec("Png", "balanced", 4, [](const ffpsd::ImageView& image) { return ffpsd::EncodePng(image); }, png);
        AddCodec(
            "Png", "fastest", 4, [](const ffpsd::ImageView& image) { return ffpsd::EncodePng(image, ffpsd::PngCompression::kFastest); },
            png);
#endif
#if defined(FFPSD_HAS_JPEG)
        AddCodec(
            "Jpeg", "quality_90", 3, [](const ffpsd::ImageView& image) { return ffpsd::EncodeJpeg(image); },
            [](const std::vector<std::uint8_t>& file) { return ffpsd::LoadJpeg(file.data(), file.size(), ffpsd::ColorMode::kRgb, 8); });
#endif
#if defined(FFPSD_HAS_WEBP)
        const Load webp = [](const std::vector<std::uint8_t>& file) {
            return ffpsd::LoadWebp(file.data(), file.size(), ffpsd::ColorMode::kRgb, 8);
        };
        AddCodec("Webp", "lossy", 3, [](const ffpsd::ImageView& image) { return ffpsd::EncodeWebp(image); }, webp);
        AddCodec("Webp", "lossless", 4, [](const ffpsd::ImageView& image) { return ffpsd::EncodeWebp(image, 50, true); }, webp);
#endif
        return true;
    }();
} // namespace
