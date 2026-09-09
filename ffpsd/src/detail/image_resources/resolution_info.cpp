#include "detail/image_resources/resolution_info.hpp"

#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fixed.hpp"

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kBlockSize = 16;
    } // namespace

    ResolutionInfo GetResolutionInfo(const ImageResources& image_resources)
    {
        ResolutionInfo value;

        const std::size_t at = FindImageResourceIndex(image_resources, ImageResourceId::kResolutionInfo);
        if (at == kNoImageResource || image_resources[at]->data.size() < kBlockSize)
            return value;

        BigEndianReader reader(image_resources[at]->data);
        value.horizontal = FixedToDouble(reader.ReadU32());
        value.horizontal_unit = reader.ReadI16();
        value.width_unit = reader.ReadI16();
        value.vertical = FixedToDouble(reader.ReadU32());
        value.vertical_unit = reader.ReadI16();
        value.height_unit = reader.ReadI16();

        return value;
    }

    void SetResolutionInfo(ImageResources& image_resources, ResolutionInfo value)
    {
        BigEndianWriter writer(kBlockSize);
        writer.WriteU32(DoubleToFixed(value.horizontal));
        writer.WriteI16(value.horizontal_unit);
        writer.WriteI16(value.width_unit);
        writer.WriteU32(DoubleToFixed(value.vertical));
        writer.WriteI16(value.vertical_unit);
        writer.WriteI16(value.height_unit);

        FindOrInsertImageResource(image_resources, ImageResourceId::kResolutionInfo).data = writer.Take();
    }
} // namespace ffpsd::detail
