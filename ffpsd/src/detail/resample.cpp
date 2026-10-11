#include "detail/resample.hpp"

#include "detail/color.hpp"
#include "detail/image.hpp"
#include "detail/samples.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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

        // Rounded half away from zero, as std::lround, for v in 0 to Full: a float plus 0.5 is exact in a double.
        template <typename T> T FromFloat(float value) noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
                return value;
            else
                return static_cast<T>(static_cast<double>(std::clamp(value, 0.0f, static_cast<float>(Full<T>()))) + 0.5);
        }

        // Output pixel x of a row across: its weights times the row's samples, in tap order.
        float FilterPixel(const float* row, const Taps& across, std::size_t x) noexcept
        {
            const float* weights = across.weights.data() + x * across.width;
            const float* samples = row + across.first[x];
            float sum = 0.0f;
            for (std::size_t k = 0; k < across.count[x]; ++k)
                sum += weights[k] * samples[k];
            return sum;
        }

        // Four pixels with as many taps at once, as independent sums: each adds in FilterPixel's order, so to the bit.
        void FilterAcross(const float* row, const Taps& across, float* out) noexcept
        {
            constexpr std::size_t kGroup = 4;
            const std::size_t size = across.first.size();
            std::size_t x = 0;
            for (; x + kGroup <= size; x += kGroup)
            {
                const std::size_t count = across.count[x];
                if (!std::all_of(&across.count[x], &across.count[x] + kGroup, [count](std::size_t c) { return c == count; }))
                {
                    for (std::size_t i = x; i < x + kGroup; ++i)
                        out[i] = FilterPixel(row, across, i);
                    continue;
                }

                const float* weights[kGroup];
                const float* samples[kGroup];
                float sums[kGroup] = {};
                for (std::size_t i = 0; i < kGroup; ++i)
                {
                    weights[i] = across.weights.data() + (x + i) * across.width;
                    samples[i] = row + across.first[x + i];
                }
                for (std::size_t k = 0; k < count; ++k)
                {
                    for (std::size_t i = 0; i < kGroup; ++i)
                        sums[i] += weights[i][k] * samples[i][k];
                }
                std::copy(sums, sums + kGroup, out + x);
            }
            for (; x < size; ++x)
                out[x] = FilterPixel(row, across, x);
        }

        // A row at a time: source rows filtered across wait in a ring for the pass down, so no whole plane is held as floats.
        template <typename T> void CubicImage(const Image& image, Image& result, bool planes_alone)
        {
            const std::size_t channels = image.channel_count;
            const std::size_t in_width = image.width;
            const std::size_t out_width = result.width;
            const std::size_t in_plane = in_width * image.height * sizeof(T);
            const std::size_t out_plane = out_width * result.height * sizeof(T);
            const bool has_alpha = !planes_alone && HasTransparency(image);
            const std::size_t color_count = has_alpha ? ColorChannelCount(image.color_mode) : 0;
            const auto full = static_cast<float>(Full<T>());

            const Taps across = MakeTaps(image.width, result.width);
            const Taps down = MakeTaps(image.height, result.height);
            const std::size_t ring = down.width;

            std::vector<float> rows(channels * ring * out_width);  // rows[channel][source row % ring][x]
            std::vector<float> line(in_width);                     // one source row as floats
            std::vector<float> coverage(has_alpha ? in_width : 0); // its alpha over full
            std::vector<float> out(channels * out_width);          // one output row of every channel
            const auto ring_row = [&](std::size_t channel, std::size_t source_y) {
                return rows.data() + (channel * ring + source_y % ring) * out_width;
            };

            std::size_t next = 0; // the source row to filter next
            for (std::size_t y = 0; y < result.height; ++y)
            {
                // Color times alpha, so a clear pixel adds no color to its neighbours.
                for (next = std::max(next, down.first[y]); next < down.first[y] + down.count[y]; ++next)
                {
                    const std::size_t row_offset = next * in_width * sizeof(T);
                    if (has_alpha)
                    {
                        const std::uint8_t* alpha = image.bytes.data() + color_count * in_plane + row_offset;
                        for (std::size_t x = 0; x < in_width; ++x)
                            coverage[x] = static_cast<float>(Load<T>(alpha + x * sizeof(T))) / full;
                    }
                    for (std::size_t c = 0; c < channels; ++c)
                    {
                        const std::uint8_t* samples = image.bytes.data() + c * in_plane + row_offset;
                        for (std::size_t x = 0; x < in_width; ++x)
                            line[x] = static_cast<float>(Load<T>(samples + x * sizeof(T)));
                        if (c < color_count)
                        {
                            for (std::size_t x = 0; x < in_width; ++x)
                                line[x] *= coverage[x];
                        }
                        FilterAcross(line.data(), across, ring_row(c, next));
                    }
                }

                const float* weights = down.weights.data() + y * down.width;
                for (std::size_t c = 0; c < channels; ++c)
                {
                    float* row = out.data() + c * out_width;
                    std::fill(row, row + out_width, 0.0f);
                    for (std::size_t k = 0; k < down.count[y]; ++k)
                    {
                        const float* source = ring_row(c, down.first[y] + k);
                        const float weight = weights[k];
                        for (std::size_t x = 0; x < out_width; ++x)
                            row[x] += weight * source[x];
                    }
                }

                if (has_alpha)
                {
                    const float* alpha = out.data() + color_count * out_width;
                    for (std::size_t c = 0; c < color_count; ++c)
                    {
                        float* row = out.data() + c * out_width;
                        for (std::size_t x = 0; x < out_width; ++x)
                            row[x] = alpha[x] > 0.0f ? row[x] * full / alpha[x] : 0.0f;
                    }
                }

                for (std::size_t c = 0; c < channels; ++c)
                {
                    const float* row = out.data() + c * out_width;
                    std::uint8_t* to = result.bytes.data() + c * out_plane + y * out_width * sizeof(T);
                    for (std::size_t x = 0; x < out_width; ++x)
                        Store<T>(to + x * sizeof(T), FromFloat<T>(row[x]));
                }
            }
        }

        // The source pixel whose center is nearest; integer arithmetic, so it never drifts.
        std::size_t NearestSource(std::size_t out_index, std::size_t in, std::size_t out) noexcept
        {
            return std::min(static_cast<std::size_t>((std::uint64_t{2} * out_index + 1) * in / (std::uint64_t{2} * out)), in - 1);
        }

        // Whole pixels are copied, never mixed, so transparency needs no care; a float moves as its bits.
        template <typename T> void NearestImage(const Image& image, Image& result)
        {
            const std::size_t in_plane = std::size_t{image.width} * image.height * sizeof(T);
            const std::size_t out_plane = std::size_t{result.width} * result.height * sizeof(T);
            const std::size_t out_row = result.width * sizeof(T);

            std::vector<std::size_t> columns(result.width);
            for (std::size_t x = 0; x < result.width; ++x)
                columns[x] = NearestSource(x, image.width, result.width) * sizeof(T);

            for (std::size_t c = 0; c < image.channel_count; ++c)
            {
                std::size_t previous = static_cast<std::size_t>(-1);
                for (std::size_t y = 0; y < result.height; ++y)
                {
                    std::uint8_t* to = result.bytes.data() + c * out_plane + y * out_row;
                    const std::size_t source_y = NearestSource(y, image.height, result.height);

                    // Enlarging repeats source rows: the row above is the same one.
                    if (source_y == previous)
                    {
                        std::memcpy(to, to - out_row, out_row);
                        continue;
                    }
                    previous = source_y;

                    const std::uint8_t* row = image.bytes.data() + c * in_plane + source_y * image.width * sizeof(T);
                    for (std::size_t x = 0; x < result.width; ++x)
                        Store<T>(to + x * sizeof(T), Load<T>(row + columns[x]));
                }
            }
        }

        template <typename T> void ResampleAs(const Image& image, Image& result, ResampleFilter filter, bool planes_alone)
        {
            if (filter == ResampleFilter::kNearest)
                NearestImage<T>(image, result);
            else
                CubicImage<T>(image, result, planes_alone);
        }

        // Quarter turns and mirrors: every pixel moves whole, a float as its bits.
        template <typename T> void TurnImage(const Image& image, Image& result, bool transpose, bool flip_x, bool flip_y)
        {
            const std::size_t plane = std::size_t{image.width} * image.height * sizeof(T);
            const std::size_t out_width = result.width;
            const std::size_t out_height = result.height;
            for (std::size_t c = 0; c < image.channel_count; ++c)
            {
                const std::uint8_t* in = image.bytes.data() + c * plane;
                std::uint8_t* out = result.bytes.data() + c * plane;
                if (!transpose)
                {
                    for (std::size_t y = 0; y < out_height; ++y)
                    {
                        const std::uint8_t* row = in + (flip_y ? out_height - 1 - y : y) * out_width * sizeof(T);
                        std::uint8_t* to = out + y * out_width * sizeof(T);
                        if (!flip_x)
                        {
                            std::memcpy(to, row, out_width * sizeof(T));
                            continue;
                        }
                        for (std::size_t x = 0; x < out_width; ++x)
                            Store<T>(to + x * sizeof(T), Load<T>(row + (out_width - 1 - x) * sizeof(T)));
                    }
                    continue;
                }

                // A transpose reads down the columns: tiles keep what is read and what is written in the cache.
                constexpr std::size_t kTile = 64;
                for (std::size_t tile_y = 0; tile_y < out_height; tile_y += kTile)
                {
                    for (std::size_t tile_x = 0; tile_x < out_width; tile_x += kTile)
                    {
                        for (std::size_t y = tile_y; y < std::min(tile_y + kTile, out_height); ++y)
                        {
                            const std::size_t down = flip_y ? out_height - 1 - y : y;
                            for (std::size_t x = tile_x; x < std::min(tile_x + kTile, out_width); ++x)
                            {
                                const std::size_t across = flip_x ? out_width - 1 - x : x;
                                Store<T>(out + (y * out_width + x) * sizeof(T), Load<T>(in + (across * image.width + down) * sizeof(T)));
                            }
                        }
                    }
                }
            }
        }
    } // namespace

    Image Resample(const Image& image, std::uint32_t width, std::uint32_t height, ResampleFilter filter, bool planes_alone)
    {
        if (image.width == width && image.height == height)
            return image;

        ImageInfo info = image;
        info.width = width;
        info.height = height;
        Image result(info);

        switch (image.depth)
        {
        case 8:
            ResampleAs<std::uint8_t>(image, result, filter, planes_alone);
            break;
        case 16:
            ResampleAs<std::uint16_t>(image, result, filter, planes_alone);
            break;
        default:
            ResampleAs<float>(image, result, filter, planes_alone);
            break;
        }
        return result;
    }

    Image TransformPixels(
        Image image, const Transform& transform, std::uint32_t width, std::uint32_t height, ResampleFilter filter, bool planes_alone)
    {
        const bool transpose = transform.SwapsAxes();
        const bool flip_x = (transpose ? transform.c : transform.a) < 0;
        const bool flip_y = (transpose ? transform.b : transform.d) < 0;
        if (transpose || flip_x || flip_y)
        {
            ImageInfo info = image;
            if (transpose)
                std::swap(info.width, info.height);
            Image turned(info);
            switch (image.depth)
            {
            case 8:
                TurnImage<std::uint8_t>(image, turned, transpose, flip_x, flip_y);
                break;
            case 16:
                TurnImage<std::uint16_t>(image, turned, transpose, flip_x, flip_y);
                break;
            default:
                TurnImage<float>(image, turned, transpose, flip_x, flip_y);
                break;
            }
            image = std::move(turned);
        }
        if (image.width == width && image.height == height)
            return image;
        return Resample(image, width, height, filter, planes_alone);
    }
} // namespace ffpsd::detail
