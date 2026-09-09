#ifndef FFPSD_DETAIL_IO_FILE_HPP_
#define FFPSD_DETAIL_IO_FILE_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // Both throw std::system_error, or std::filesystem::filesystem_error for a missing file.
    std::vector<std::uint8_t> ReadFile(const std::string& path);
    void WriteFile(const std::string& path, const std::vector<std::uint8_t>& data);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_FILE_HPP_
