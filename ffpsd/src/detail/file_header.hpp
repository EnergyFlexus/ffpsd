#ifndef FFPSD_DETAIL_FILE_HEADER_HPP_
#define FFPSD_DETAIL_FILE_HEADER_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstdint>
#include <ffpsd/image.hpp>
#include <ffpsd/types.hpp>
#include <string>

namespace ffpsd::detail
{
    constexpr std::uint16_t kVersionPsd = 1;
    constexpr std::uint16_t kVersionPsb = 2;
    constexpr std::uint16_t kMaxChannels = 56;

    constexpr std::uint32_t kMaxSidePsd = 30000;
    constexpr std::uint32_t kMaxSidePsb = 300000;

    constexpr std::uint32_t MaxSide(bool is_psb) noexcept
    {
        return is_psb ? kMaxSidePsb : kMaxSidePsd;
    }

    // 1 bit bitmaps and the sample depths.
    constexpr bool IsDepth(std::uint16_t depth) noexcept
    {
        return depth == 1 || depth == 8 || depth == 16 || depth == 32;
    }

    // The modes the format defines; 5 and 6 are none.
    constexpr bool IsColorMode(ColorMode color_mode) noexcept
    {
        switch (color_mode)
        {
        case ColorMode::kBitmap:
        case ColorMode::kGrayscale:
        case ColorMode::kIndexed:
        case ColorMode::kRgb:
        case ColorMode::kCmyk:
        case ColorMode::kMultichannel:
        case ColorMode::kDuotone:
        case ColorMode::kLab:
            return true;
        default:
            return false;
        }
    }

    // Bitmap is 1 bit and nothing else is. Error is invalid_argument for a caller's document and runtime_error for a file's.
    template <class Error> void CheckDepthAndMode(std::uint16_t depth, ColorMode color_mode)
    {
        if (!IsDepth(depth))
            throw Error("ffpsd: unsupported depth: " + std::to_string(depth));
        if (!IsColorMode(color_mode))
            throw Error("ffpsd: unknown color mode " + std::to_string(static_cast<int>(color_mode)));
        if ((color_mode == ColorMode::kBitmap) != (depth == 1))
            throw Error(
                "ffpsd: only a bitmap is 1 bit, and it is nothing else; not color mode " + std::to_string(static_cast<int>(color_mode)) +
                " at " + std::to_string(depth) + " bit");
    }

    // Error is invalid_argument for a caller's size and runtime_error for a file's.
    template <class Error> void CheckCanvasSides(std::uint32_t width, std::uint32_t height, bool is_psb)
    {
        const std::uint32_t max_side = MaxSide(is_psb);
        if (width < 1 || width > max_side || height < 1 || height > max_side)
            throw Error(
                "ffpsd: a " + std::to_string(width) + " x " + std::to_string(height) + " canvas, sides are 1 to " +
                std::to_string(max_side));
    }

    // A layer may be no wider or taller than the document could be.
    void CheckLayerSides(std::uint32_t width, std::uint32_t height, bool is_psb);

    // Section 1, a fixed 26 bytes at the start of every file.
    struct FileHeader
    {
        std::uint16_t version = kVersionPsd;
        // The canvas and the composite's channels, alpha ones included.
        ImageInfo info;
    };
    // Throws std::runtime_error for whatever no PSD or PSB holds.
    FileHeader ParseFileHeader(BigEndianReader& reader);
    void WriteFileHeader(BigEndianWriter& writer, const FileHeader& header);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_FILE_HEADER_HPP_
