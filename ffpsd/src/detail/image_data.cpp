#include "detail/image_data.hpp"

#include "detail/image.hpp"
#include "detail/samples.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    PixelData
    ParseImageData(BigEndianReader& reader, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth)
    {
        std::vector<std::uint8_t> bytes = reader.ReadBytes(reader.GetRemaining());
        return PixelData(std::move(bytes), std::size_t{height} * channel_count, RowBytes(width, depth), SampleBytes(depth));
    }

    ImageInfo ImageDataInfo(
        const PixelData& data, std::uint32_t width, std::uint32_t height, std::uint16_t channel_count, std::uint16_t depth,
        ColorMode color_mode)
    {
        if (data.IsEmpty())
            return ImageInfo();
        if (!IsSampleDepth(depth))
            throw std::runtime_error(UnsupportedDepth(depth));
        return ImageInfo{width, height, channel_count, depth, color_mode};
    }
} // namespace ffpsd::detail
