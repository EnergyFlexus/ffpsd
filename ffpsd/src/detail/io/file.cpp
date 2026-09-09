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
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        if (ec)
            throw std::filesystem::filesystem_error("ffpsd: cannot open", path, ec);

        std::ifstream file(path, std::ios::binary);
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
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot create " + path);

        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        file.close();
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot write " + path);
    }
} // namespace ffpsd::detail
