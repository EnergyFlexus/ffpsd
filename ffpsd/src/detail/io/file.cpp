#include "detail/io/file.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>

namespace ffpsd::detail
{
    std::vector<std::uint8_t> ReadFile(const std::string& path)
    {
        // UTF-8 whatever the system's code page: Windows gets the path as UTF-16.
        const std::filesystem::path file_path = std::filesystem::u8path(path);

        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(file_path, ec);
        if (ec)
            throw std::filesystem::filesystem_error("ffpsd: cannot open", file_path, ec);

        std::ifstream file(file_path, std::ios::binary);
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot open " + path);

        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot read " + path);
        return data;
    }

    void WriteFile(const std::string& path, const std::vector<std::uint8_t>& data)
    {
        std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot create " + path);

        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        file.close();
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot write " + path);
    }
} // namespace ffpsd::detail
