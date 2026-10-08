#include "detail/formats/planes.hpp"

#include "detail/image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#if defined(_M_X64) || defined(__x86_64__)
#define FFPSD_SPLIT_SSSE3
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <tmmintrin.h>
#endif
#if defined(__GNUC__) || defined(__clang__)
#define FFPSD_TARGET_SSSE3 __attribute__((target("ssse3")))
#else
#define FFPSD_TARGET_SSSE3
#endif
#elif defined(_M_ARM64) || defined(__aarch64__)
#define FFPSD_SPLIT_NEON
#include <arm_neon.h>
#endif

namespace ffpsd::detail
{
    namespace
    {
        // Where source pixel (x, y) lands in a plane: origin + x * step_x + y * step_y.
        struct Steps
        {
            std::ptrdiff_t origin = 0;
            std::ptrdiff_t step_x = 1;
            std::ptrdiff_t step_y = 0;
        };

        bool SwapsSides(Orientation orientation) noexcept
        {
            return orientation >= Orientation::kTranspose;
        }

        Steps StepsOf(Orientation orientation, std::uint32_t width, std::uint32_t height) noexcept
        {
            const auto w = static_cast<std::ptrdiff_t>(width);
            const auto h = static_cast<std::ptrdiff_t>(height);
            const std::ptrdiff_t out_width = SwapsSides(orientation) ? h : w;
            switch (orientation)
            {
            case Orientation::kMirrorHorizontal:
                return {w - 1, -1, out_width};
            case Orientation::kRotate180:
                return {(h - 1) * out_width + w - 1, -1, -out_width};
            case Orientation::kMirrorVertical:
                return {(h - 1) * out_width, 1, -out_width};
            case Orientation::kTranspose:
                return {0, out_width, 1};
            case Orientation::kRotate90:
                return {h - 1, out_width, -1};
            case Orientation::kTransverse:
                return {(w - 1) * out_width + h - 1, -out_width, -1};
            case Orientation::kRotate270:
                return {(w - 1) * out_width, -out_width, 1};
            default:
                return {0, 1, out_width};
            }
        }

        template <std::size_t kSample>
        void Scatter(
            const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channels, Steps steps,
            std::uint8_t* planes)
        {
            const std::size_t plane_bytes = std::size_t{width} * height * kSample;
            const std::uint8_t* in = pixels;
            for (std::uint32_t y = 0; y < height; ++y)
            {
                std::ptrdiff_t at = steps.origin + static_cast<std::ptrdiff_t>(y) * steps.step_y;
                for (std::uint32_t x = 0; x < width; ++x, at += steps.step_x)
                {
                    std::uint8_t* out = planes + static_cast<std::size_t>(at) * kSample;
                    for (std::uint16_t channel = 0; channel < channels; ++channel, in += kSample)
                        std::memcpy(out + channel * plane_bytes, in, kSample);
                }
            }
        }

#if defined(FFPSD_SPLIT_SSSE3)
        // SSSE3 is missing from some x86-64 processors, so its loop runs only where cpuid reports it.
        bool HasSsse3() noexcept
        {
            static const bool has = [] {
#if defined(_MSC_VER)
                int registers[4] = {};
                __cpuid(registers, 1);
                return (registers[2] & (1 << 9)) != 0;
#else
                unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
                return __get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 && (ecx & (1u << 9)) != 0;
#endif
            }();
            return has;
        }

        // For 16 pixels: which byte of each 16 byte block holds a channel's sample, 0x80 where none does.
        template <std::uint16_t kChannels> struct SplitMasks
        {
            alignas(16) std::int8_t bytes[kChannels][kChannels][16];

            SplitMasks() noexcept
            {
                for (int channel = 0; channel < kChannels; ++channel)
                    for (int block = 0; block < kChannels; ++block)
                        for (int pixel = 0; pixel < 16; ++pixel)
                        {
                            const int at = kChannels * pixel + channel - 16 * block;
                            bytes[channel][block][pixel] = static_cast<std::int8_t>(at >= 0 && at < 16 ? at : -128);
                        }
            }
        };

        // 16 pixels a step; returns how many it split, the rest being left to the plain loop.
        template <std::uint16_t kChannels>
        FFPSD_TARGET_SSSE3 std::uint32_t
        SplitRowSimd(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out)
        {
            static const SplitMasks<kChannels> masks;
            std::uint32_t x = 0;
            for (; x + 16 <= width; x += 16)
            {
                __m128i blocks[kChannels];
                for (int block = 0; block < kChannels; ++block)
                    blocks[block] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + std::size_t{x} * kChannels + 16 * block));
                for (int channel = 0; channel < kChannels; ++channel)
                {
                    __m128i plane = _mm_setzero_si128();
                    for (int block = 0; block < kChannels; ++block)
                    {
                        const __m128i mask = _mm_load_si128(reinterpret_cast<const __m128i*>(masks.bytes[channel][block]));
                        plane = _mm_or_si128(plane, _mm_shuffle_epi8(blocks[block], mask));
                    }
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + channel * plane_bytes + x), plane);
                }
            }
            return x;
        }
#elif defined(FFPSD_SPLIT_NEON)
        // 16 pixels a step; returns how many it split, the rest being left to the plain loop.
        template <std::uint16_t kChannels>
        std::uint32_t SplitRowSimd(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out)
        {
            std::uint32_t x = 0;
            for (; x + 16 <= width; x += 16)
            {
                if constexpr (kChannels == 3)
                {
                    const uint8x16x3_t pixels = vld3q_u8(in + std::size_t{x} * 3);
                    for (int channel = 0; channel < 3; ++channel)
                        vst1q_u8(out + channel * plane_bytes + x, pixels.val[channel]);
                }
                else
                {
                    const uint8x16x4_t pixels = vld4q_u8(in + std::size_t{x} * 4);
                    for (int channel = 0; channel < 4; ++channel)
                        vst1q_u8(out + channel * plane_bytes + x, pixels.val[channel]);
                }
            }
            return x;
        }
#endif

        // A row goes out one plane after another, so the stores run straight and the row stays in cache.
        template <std::size_t kSample, std::uint16_t kChannels>
        void SplitRow(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out)
        {
            std::uint32_t done = 0;
            if constexpr (kSample == 1 && (kChannels == 3 || kChannels == 4))
            {
#if defined(FFPSD_SPLIT_SSSE3)
                if (HasSsse3())
                    done = SplitRowSimd<kChannels>(in, width, plane_bytes, out);
#elif defined(FFPSD_SPLIT_NEON)
                done = SplitRowSimd<kChannels>(in, width, plane_bytes, out);
#endif
            }
            for (std::uint16_t channel = 0; channel < kChannels; ++channel, out += plane_bytes)
            {
                for (std::uint32_t x = done; x < width; ++x)
                    std::memcpy(out + x * kSample, in + (x * kChannels + channel) * kSample, kSample);
            }
        }

        template <std::size_t kSample>
        void SplitRow(const std::uint8_t* in, std::uint32_t width, std::uint16_t channels, std::size_t plane_bytes, std::uint8_t* out)
        {
            switch (channels)
            {
            case 1:
                return SplitRow<kSample, 1>(in, width, plane_bytes, out);
            case 2:
                return SplitRow<kSample, 2>(in, width, plane_bytes, out);
            case 3:
                return SplitRow<kSample, 3>(in, width, plane_bytes, out);
            case 4:
                return SplitRow<kSample, 4>(in, width, plane_bytes, out);
            default:
                for (std::uint16_t channel = 0; channel < channels; ++channel, out += plane_bytes)
                {
                    for (std::uint32_t x = 0; x < width; ++x)
                        std::memcpy(out + x * kSample, in + (std::size_t{x} * channels + channel) * kSample, kSample);
                }
            }
        }
        // SplitRow backwards: a sample size known at compile time makes each copy a plain move instead of a call.
        template <std::size_t kSample, std::uint16_t kChannels>
        void JoinRow(const std::uint8_t* in, std::uint32_t width, std::size_t plane_bytes, std::uint8_t* out) noexcept
        {
            for (std::uint16_t channel = 0; channel < kChannels; ++channel, in += plane_bytes)
            {
                for (std::uint32_t x = 0; x < width; ++x)
                    std::memcpy(out + (x * kChannels + channel) * kSample, in + x * kSample, kSample);
            }
        }

        template <std::size_t kSample>
        void
        JoinRow(const std::uint8_t* in, std::uint32_t width, std::uint16_t channels, std::size_t plane_bytes, std::uint8_t* out) noexcept
        {
            switch (channels)
            {
            case 1:
                return JoinRow<kSample, 1>(in, width, plane_bytes, out);
            case 2:
                return JoinRow<kSample, 2>(in, width, plane_bytes, out);
            case 3:
                return JoinRow<kSample, 3>(in, width, plane_bytes, out);
            case 4:
                return JoinRow<kSample, 4>(in, width, plane_bytes, out);
            default:
                for (std::uint16_t channel = 0; channel < channels; ++channel, in += plane_bytes)
                {
                    for (std::uint32_t x = 0; x < width; ++x)
                        std::memcpy(out + (std::size_t{x} * channels + channel) * kSample, in + x * kSample, kSample);
                }
            }
        }
    } // namespace

    Image MakePlanes(std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth)
    {
        if (!IsSampleDepth(depth))
            throw std::invalid_argument(UnsupportedDepth(depth));

        return MakeImage(width, height, channel_count, depth, channel_count >= 3 ? ColorMode::kRgb : ColorMode::kGrayscale);
    }

    void DeinterleaveRow(const std::uint8_t* row, Image& image, std::uint32_t y)
    {
        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t plane_bytes = row_bytes * image.height;
        std::uint8_t* out = image.bytes.data() + y * row_bytes;
        switch (image.GetBytesPerSample())
        {
        case 1:
            return SplitRow<1>(row, image.width, image.channel_count, plane_bytes, out);
        case 2:
            return SplitRow<2>(row, image.width, image.channel_count, plane_bytes, out);
        default:
            return SplitRow<4>(row, image.width, image.channel_count, plane_bytes, out);
        }
    }

    Image Deinterleave(
        const std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        Orientation orientation)
    {
        const bool swaps = SwapsSides(orientation);
        Image image = MakePlanes(swaps ? height : width, swaps ? width : height, channel_count, depth);

        const std::size_t sample = image.GetBytesPerSample();
        if (orientation == Orientation::kNormal)
        {
            for (std::uint32_t y = 0; y < height; ++y)
                DeinterleaveRow(pixels + y * std::size_t{width} * channel_count * sample, image, y);
            return image;
        }

        const Steps steps = StepsOf(orientation, width, height);
        switch (sample)
        {
        case 1:
            Scatter<1>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        case 2:
            Scatter<2>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        default:
            Scatter<4>(pixels, width, height, channel_count, steps, image.bytes.data());
            break;
        }
        return image;
    }

    void InterleaveRow(const ImageView& image, std::uint16_t channel_count, std::uint32_t y, std::uint8_t* out) noexcept
    {
        const std::size_t row_bytes = std::size_t{image.width} * image.GetBytesPerSample();
        const std::size_t plane_bytes = row_bytes * image.height;
        const std::uint8_t* in = image.data + y * row_bytes;
        switch (image.GetBytesPerSample())
        {
        case 1:
            return JoinRow<1>(in, image.width, channel_count, plane_bytes, out);
        case 2:
            return JoinRow<2>(in, image.width, channel_count, plane_bytes, out);
        default:
            return JoinRow<4>(in, image.width, channel_count, plane_bytes, out);
        }
    }
} // namespace ffpsd::detail
