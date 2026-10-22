#include "detail/image_data.hpp"

#include "detail/limits.hpp"
#include "detail/samples.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace ffpsd::detail
{
    PixelData ParseImageData(BigEndianReader& reader, const ImageInfo& info)
    {
        std::vector<std::uint8_t> bytes = reader.ReadBytes(reader.GetRemaining());
        return PixelData(
            std::move(bytes), std::size_t{info.height} * info.channel_count, RowBytes(info.width, info.depth), SampleBytes(info.depth));
    }

    void WriteImageData(BigEndianWriter& writer, const PixelData& data)
    {
        writer.WriteBytes(data.GetBytes());
    }

    ImageInfo ImageDataInfo(const PixelData& data, const ImageInfo& info)
    {
        if (data.IsEmpty())
            return ImageInfo();
        CheckSampleDepth<std::runtime_error>(info.depth);
        return info;
    }
} // namespace ffpsd::detail
