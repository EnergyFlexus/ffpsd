#ifndef FFPSD_DETAIL_COLOR_MODE_DATA_HPP_
#define FFPSD_DETAIL_COLOR_MODE_DATA_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstdint>
#include <vector>

namespace ffpsd::detail
{
    std::vector<std::uint8_t> ParseColorModeData(BigEndianReader& reader);
    void WriteColorModeData(BigEndianWriter& writer, const std::vector<std::uint8_t>& data);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_COLOR_MODE_DATA_HPP_
