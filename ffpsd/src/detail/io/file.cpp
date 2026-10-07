#include "detail/io/file.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <share.h>
#include <windows.h>
#endif

namespace ffpsd::detail
{
    namespace
    {
        struct FileCloser
        {
            void operator()(std::FILE* file) const noexcept
            {
                std::fclose(file);
            }
        };
        using File = std::unique_ptr<std::FILE, FileCloser>;

        // fread and fwrite move a large file several times faster than MSVC's streams.
        File OpenFile(const std::filesystem::path& path, bool write)
        {
#if defined(_WIN32)
            // UTF-16 for Windows, shared as fopen shares, so a file another program holds open still reads.
            return File(_wfsopen(path.c_str(), write ? L"wb" : L"rb", _SH_DENYNO));
#else
            return File(std::fopen(path.c_str(), write ? "wb" : "rb"));
#endif
        }
    } // namespace

    FileData ReadFile(const std::string& path)
    {
        // UTF-8 whatever the system's code page: Windows gets the path as UTF-16.
        const std::filesystem::path file_path = std::filesystem::u8path(path);

        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(file_path, ec);
        if (ec)
            throw std::filesystem::filesystem_error("ffpsd: cannot open", file_path, ec);

        const File file = OpenFile(file_path, false);
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot open " + path);

        FileData data;
        data.size = static_cast<std::size_t>(size);
        data.bytes.reset(new std::uint8_t[data.size]);
        if (data.size != 0 && std::fread(data.bytes.get(), 1, data.size, file.get()) != data.size)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot read " + path);
        return data;
    }

    void WriteFile(const std::string& path, const std::vector<std::uint8_t>& data)
    {
#if defined(_WIN32)
        // MSVC's fwrite writes a large buffer at half the speed of WriteFile, so Windows gets WriteFile itself.
        const HANDLE file = ::CreateFileW(
            std::filesystem::u8path(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "ffpsd: cannot create " + path);

        // A call takes a 32 bit count, so a PSB goes in pieces.
        constexpr std::size_t kPiece = std::size_t{1} << 24;
        DWORD error = 0;
        for (std::size_t at = 0; at < data.size() && error == 0;)
        {
            const auto piece = static_cast<DWORD>(std::min(data.size() - at, kPiece));
            DWORD written = 0;
            if (!::WriteFile(file, data.data() + at, piece, &written, nullptr))
                error = ::GetLastError();
            else if (written != piece)
                error = ERROR_WRITE_FAULT;
            at += written;
        }
        if (!::CloseHandle(file) && error == 0)
            error = ::GetLastError();
        if (error != 0)
            throw std::system_error(static_cast<int>(error), std::system_category(), "ffpsd: cannot write " + path);
#else
        File file = OpenFile(std::filesystem::u8path(path), true);
        if (!file)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot create " + path);

        // Closing flushes, so a full disk shows there too.
        const bool written = data.empty() || std::fwrite(data.data(), 1, data.size(), file.get()) == data.size();
        if (std::fclose(file.release()) != 0 || !written)
            throw std::system_error(errno, std::generic_category(), "ffpsd: cannot write " + path);
#endif
    }
} // namespace ffpsd::detail
