#ifndef FFPSD_DETAIL_IO_COMPRESSION_HPP_
#define FFPSD_DETAIL_IO_COMPRESSION_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    // PackBits, the RLE of PSD and TIFF; fills exactly out_size bytes or throws.
    void UnpackBits(const std::uint8_t* data, std::size_t size, std::uint8_t* out, std::size_t out_size);

    // Runs of three or more repeat, everything else is literal; appends to out.
    void PackBits(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_COMPRESSION_HPP_
