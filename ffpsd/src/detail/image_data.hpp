#ifndef FFPSD_DETAIL_IMAGE_DATA_HPP_
#define FFPSD_DETAIL_IMAGE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/pixel_data.hpp"

#include <ffpsd/image.hpp>

namespace ffpsd::detail
{
    // Section 5, the merged image, as stored: the rest of the file, every channel's rows in one run.
    PixelData ParseImageData(BigEndianReader& reader, const ImageInfo& info);
    void WriteImageData(BigEndianWriter& writer, const PixelData& data);

    // The composite's shape; empty when the file has none.
    ImageInfo ImageDataInfo(const PixelData& data, const ImageInfo& info);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IMAGE_DATA_HPP_
