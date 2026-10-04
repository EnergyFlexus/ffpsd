#include "detail/resample.hpp"

#include "detail/color.hpp"
#include "detail/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    namespace
    {
        // Keys' cubic with a = -0.5, Catmull-Rom: it passes through the samples and keeps a ramp a ramp.
        constexpr double kCubicA = -0.5;
        constexpr double kCubicRadius = 2.0;

        double Cubic(double x) noexcept
        {
            x = std::abs(x);
            if (x < 1.0)
                return ((kCubicA + 2.0) * x - (kCubicA + 3.0)) * x * x + 1.0;
            if (x < 2.0)
                return ((kCubicA * x - 5.0 * kCubicA) * x + 8.0 * kCubicA) * x - 4.0 * kCubicA;
            return 0.0;
        }

        // Per output pixel of one axis: its first source pixel and weights; shrinking widens the kernel.
        struct Taps
        {
            std::size_t width = 0;
            std::vector<std::size_t> first;
            std::vector<std::size_t> count;
            std::vector<float> weights; // width per output pixel
        };

        Taps MakeTaps(std::size_t in, std::size_t out)
        {
            const double scale = static_cast<double>(in) / static_cast<double>(out);
            const double stretch = std::max(scale, 1.0);
            const double support = kCubicRadius * stretch;

            Taps taps;
            taps.width = static_cast<std::size_t>(std::ceil(support)) * 2 + 1;
            taps.first.resize(out);
            taps.count.resize(out);
            taps.weights.assign(out * taps.width, 0.0f);

            for (std::size_t i = 0; i < out; ++i)
            {
                const double center = (static_cast<double>(i) + 0.5) * scale;
                const auto low = static_cast<std::ptrdiff_t>(std::floor(center - support + 0.5));
                const auto high = static_cast<std::ptrdiff_t>(std::floor(center + support + 0.5));
                const std::size_t first = static_cast<std::size_t>(std::max<std::ptrdiff_t>(low, 0));
                const std::size_t end = std::min(static_cast<std::size_t>(std::max<std::ptrdiff_t>(high, 0)), in);

                // Pixels past the edge are left out and the rest renormalised, so the edge is not darkened.
                double total = 0.0;
                std::vector<double> row(end - first);
                for (std::size_t k = 0; k < row.size(); ++k)
                {
                    row[k] = Cubic((static_cast<double>(first + k) + 0.5 - center) / stretch);
                    total += row[k];
                }

                taps.first[i] = first;
                taps.count[i] = row.size();
                for (std::size_t k = 0; k < row.size(); ++k)
                    taps.weights[i * taps.width + k] = static_cast<float>(total != 0.0 ? row[k] / total : 0.0);
            }
            return taps;
        }

        template <typename T> constexpr float Full() noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return 1.0f;
            else
                return static_cast<float>(std::numeric_limits<T>::max());
        }

        template <typename T> std::vector<float> ToFloat(const Image& image)
        {
            const std::size_t samples = image.bytes.size() / sizeof(T);
            std::vector<float> values(samples);
            for (std::size_t i = 0; i < samples; ++i)
            {
                T value;
                std::memcpy(&value, image.bytes.data() + i * sizeof(T), sizeof(T));
                values[i] = static_cast<float>(value);
            }
            return values;
        }

        template <typename T> void FromFloat(const std::vector<float>& values, Image& image)
        {
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                T value;
                if constexpr (std::is_floating_point_v<T>)
                    value = values[i];
                else
                    value = static_cast<T>(std::lround(std::clamp(values[i], 0.0f, Full<T>())));
                std::memcpy(image.bytes.data() + i * sizeof(T), &value, sizeof(T));
            }
        }

        // One plane through both passes: along the rows into scratch, then down the columns.
        void CubicPlane(
            const float* in, std::size_t in_width, std::size_t in_height, const Taps& across, const Taps& down, std::vector<float>& scratch,
            float* out)
        {
            const std::size_t out_width = across.first.size();
            const std::size_t out_height = down.first.size();

            scratch.assign(out_width * in_height, 0.0f);
            for (std::size_t y = 0; y < in_height; ++y)
            {
                const float* row = in + y * in_width;
                for (std::size_t x = 0; x < out_width; ++x)
                {
                    const float* weights = across.weights.data() + x * across.width;
                    float sum = 0.0f;
                    for (std::size_t k = 0; k < across.count[x]; ++k)
                        sum += weights[k] * row[across.first[x] + k];
                    scratch[y * out_width + x] = sum;
                }
            }

            for (std::size_t y = 0; y < out_height; ++y)
            {
                float* row = out + y * out_width;
                std::fill(row, row + out_width, 0.0f);
                const float* weights = down.weights.data() + y * down.width;
                for (std::size_t k = 0; k < down.count[y]; ++k)
                {
                    const float* source = scratch.data() + (down.first[y] + k) * out_width;
                    for (std::size_t x = 0; x < out_width; ++x)
                        row[x] += weights[k] * source[x];
                }
            }
        }

        template <typename T> Image CubicImage(const Image& image, Image result)
        {
            const std::size_t in_plane = std::size_t{image.width} * image.height;
            const std::size_t out_plane = std::size_t{result.width} * result.height;
            const std::size_t color_count = ColorChannelCount(image.color_mode);
            const bool has_alpha = HasTransparency(image);
            const float full = Full<T>();

            // Color times alpha, so a clear pixel adds no color to its neighbours.
            std::vector<float> in = ToFloat<T>(image);
            if (has_alpha)
            {
                const float* alpha = in.data() + color_count * in_plane;
                for (std::size_t c = 0; c < color_count; ++c)
                {
                    float* plane = in.data() + c * in_plane;
                    for (std::size_t i = 0; i < in_plane; ++i)
                        plane[i] *= alpha[i] / full;
                }
            }

            const Taps across = MakeTaps(image.width, result.width);
            const Taps down = MakeTaps(image.height, result.height);
            std::vector<float> out(out_plane * image.channel_count);
            std::vector<float> scratch;
            for (std::size_t c = 0; c < image.channel_count; ++c)
                CubicPlane(in.data() + c * in_plane, image.width, image.height, across, down, scratch, out.data() + c * out_plane);

            if (has_alpha)
            {
                const float* alpha = out.data() + color_count * out_plane;
                for (std::size_t c = 0; c < color_count; ++c)
                {
                    float* plane = out.data() + c * out_plane;
                    for (std::size_t i = 0; i < out_plane; ++i)
                        plane[i] = alpha[i] > 0.0f ? plane[i] * full / alpha[i] : 0.0f;
                }
            }

            FromFloat<T>(out, result);
            return result;
        }

        // The source pixel whose center is nearest; integer arithmetic, so it never drifts.
        std::size_t NearestSource(std::size_t out_index, std::size_t in, std::size_t out) noexcept
        {
            return std::min(static_cast<std::size_t>((std::uint64_t{2} * out_index + 1) * in / (std::uint64_t{2} * out)), in - 1);
        }

        void NearestImage(const Image& image, Image& result)
        {
            const std::size_t sample = image.GetBytesPerSample();
            const std::size_t in_plane = std::size_t{image.width} * image.height * sample;
            const std::size_t out_plane = std::size_t{result.width} * result.height * sample;

            std::vector<std::size_t> columns(result.width);
            for (std::size_t x = 0; x < result.width; ++x)
                columns[x] = NearestSource(x, image.width, result.width);

            for (std::size_t c = 0; c < image.channel_count; ++c)
            {
                for (std::size_t y = 0; y < result.height; ++y)
                {
                    const std::size_t source_y = NearestSource(y, image.height, result.height);
                    const std::uint8_t* row = image.bytes.data() + c * in_plane + source_y * image.width * sample;
                    std::uint8_t* to = result.bytes.data() + c * out_plane + y * result.width * sample;
                    for (std::size_t x = 0; x < result.width; ++x)
                        std::memcpy(to + x * sample, row + columns[x] * sample, sample);
                }
            }
        }
    } // namespace

    Image Resample(const Image& image, std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        if (image.width == width && image.height == height)
            return image;

        Image result;
        result.width = width;
        result.height = height;
        result.channel_count = image.channel_count;
        result.depth = image.depth;
        result.color_mode = image.color_mode;
        result.bytes.resize(result.GetSizeBytes());

        if (filter == ResampleFilter::kNearest)
        {
            NearestImage(image, result);
            return result;
        }

        switch (image.depth)
        {
        case 8:
            return CubicImage<std::uint8_t>(image, std::move(result));
        case 16:
            return CubicImage<std::uint16_t>(image, std::move(result));
        default:
            return CubicImage<float>(image, std::move(result));
        }
    }

    Image ResamplePlanes(const Image& image, std::uint32_t width, std::uint32_t height, ResampleFilter filter)
    {
        Image result;
        result.width = width;
        result.height = height;
        result.channel_count = image.channel_count;
        result.depth = image.depth;
        result.color_mode = image.color_mode;
        result.bytes.resize(result.GetSizeBytes());

        Image plane;
        plane.width = image.width;
        plane.height = image.height;
        plane.channel_count = 1;
        plane.depth = image.depth;
        plane.color_mode = ColorMode::kGrayscale;
        const std::size_t in_plane = plane.GetSizeBytes();
        const std::size_t out_plane = std::size_t{width} * height * image.GetBytesPerSample();
        for (std::size_t channel = 0; channel < image.channel_count; ++channel)
        {
            const auto first = image.bytes.begin() + static_cast<std::ptrdiff_t>(channel * in_plane);
            plane.bytes.assign(first, first + static_cast<std::ptrdiff_t>(in_plane));
            const Image resized = Resample(plane, width, height, filter);
            std::copy(resized.bytes.begin(), resized.bytes.end(), result.bytes.begin() + static_cast<std::ptrdiff_t>(channel * out_plane));
        }
        return result;
    }
} // namespace ffpsd::detail
