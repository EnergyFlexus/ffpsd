#ifndef FFPSD_DETAIL_IO_FILE_HPP_
#define FFPSD_DETAIL_IO_FILE_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffpsd::detail
{
    // Not a vector: one would be zeroed before the read.
    struct FileData
    {
        std::unique_ptr<std::uint8_t[]> bytes;
        std::size_t size = 0;
    };

    // Both throw std::system_error, or std::filesystem::filesystem_error for a missing file.
    FileData ReadFile(const std::string& path);
    void WriteFile(const std::string& path, const std::vector<std::uint8_t>& data);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_FILE_HPP_
