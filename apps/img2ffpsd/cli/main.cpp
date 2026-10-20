// Folders of PNG or JPEG files into PSD files: the first one as the background, every next one as a layer above it.
#include "common/convert.hpp"
#include "common/pictures.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
    using img2ffpsd::Pictures;

    struct Options
    {
        img2ffpsd::Conversion conversion;
        bool recursive = true;
        unsigned jobs = 0; // 0: one per processor
    };

    const char* const kRed = "\033[91m";
    const char* const kYellow = "\033[93m";
    const char* const kGreen = "\033[92m";
    const char* const kEnd = "\033[0m";

    // UTF-8 output, and the escape sequences of the colors and the progress bar.
    void EnableConsole()
    {
#if defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(out, &mode);
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    }

    void Pause()
    {
        std::cout << std::flush; // pause writes past the buffer of std::cout
#if defined(_WIN32)
        std::system("pause");
#else
        std::cout << "Press Enter to continue . . . " << std::flush;
        std::cin.get();
#endif
    }

    std::string Utf8(const fs::path& path)
    {
        return path.u8string();
    }

    void PrintUsage()
    {
        std::cout << "img2ffpsd-cli " << ffpsd::Version() << "\n"
                  << "usage: img2ffpsd-cli <bottom> <top> [<layer>...] <output> [--auto|--color|--gray] [--jobs N]\n"
                  << "                     [--resize nearest|bicubic] [--recursive=true|false]\n\n"
                  << "Pairs PNG or JPEG files by their path without the extension in the bottom and\n"
                  << "top folders into PSD files in output: the bottom one as the locked background,\n"
                  << "the top one as the layer above it. Each further folder adds a layer above those\n"
                  << "where it has the file. The upper layer is the composite. Every picture is\n"
                  << "resized to the top one.\n\n"
                  << "  --auto              the colors of the top picture, gray or RGB, the default\n"
                  << "  --color             RGB documents\n"
                  << "  --gray              grayscale documents\n"
                  << "  --jobs N            files converted at once, one per processor by default\n"
                  << "  --resize nearest    resizing that keeps hard pixels, the default\n"
                  << "  --resize bicubic    smooth resizing\n"
                  << "  --recursive=true    the pictures of the folders inside too, the default\n"
                  << "  --recursive=false   the pictures right in each folder only\n";
    }

    bool ParseArguments(const std::vector<std::string>& args, Options& options)
    {
        std::vector<std::string> folders;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--auto")
                options.conversion.color = img2ffpsd::ColorChoice::kAuto;
            else if (args[i] == "--color")
                options.conversion.color = img2ffpsd::ColorChoice::kColor;
            else if (args[i] == "--gray")
                options.conversion.color = img2ffpsd::ColorChoice::kGray;
            else if (args[i] == "--recursive=true")
                options.recursive = true;
            else if (args[i] == "--recursive=false")
                options.recursive = false;
            else if (args[i] == "--jobs" && i + 1 < args.size())
                options.jobs = static_cast<unsigned>(std::max(1, std::atoi(args[++i].c_str())));
            else if (args[i] == "--resize" && i + 1 < args.size())
            {
                const std::string& method = args[++i];
                if (method == "nearest")
                    options.conversion.filter = ffpsd::ResampleFilter::kNearest;
                else if (method == "bicubic")
                    options.conversion.filter = ffpsd::ResampleFilter::kBicubic;
                else
                    return false;
            }
            else if (args[i].rfind("--", 0) == 0)
                return false; // an unknown option
            else
                folders.push_back(args[i]);
        }
        if (folders.size() < 3)
            return false;

        for (std::size_t i = 0; i + 1 < folders.size(); ++i)
            options.conversion.folders.push_back(fs::u8path(folders[i]));
        options.conversion.output = fs::u8path(folders.back());
        return true;
    }

    // Prints the names that more than one picture of the folder has, 01.png and 01.jpg; true when there are none.
    bool FindPictures(const fs::path& folder, bool recursive, Pictures& pictures)
    {
        std::vector<std::vector<fs::path>> duplicates;
        pictures = img2ffpsd::FindPictures(folder, recursive, duplicates);
        if (duplicates.empty())
            return true;

        std::cout << kRed << "error: pictures with the same name in '" << Utf8(fs::absolute(folder)) << "':\n";
        for (const std::vector<fs::path>& names : duplicates)
        {
            for (const fs::path& file : names)
                std::cout << "    - " << Utf8(file) << "\n";
        }
        std::cout << kEnd << "\n";
        return false;
    }

    // Prints the files of one folder that the other lacks; true when there are none.
    bool CheckMissing(const fs::path& other_folder, const Pictures& pictures, const Pictures& other)
    {
        const std::vector<fs::path> only = img2ffpsd::OnlyIn(pictures, other);
        if (only.empty())
            return true;

        std::cout << kRed << "error: files missing in '" << Utf8(fs::absolute(other_folder)) << "':\n";
        for (const fs::path& file : only)
            std::cout << "    - " << Utf8(file) << "\n";
        std::cout << kEnd << "\n";
        return false;
    }

    // The bottom's paths, which the top must match, and every folder's files; false, after printing why, if they cannot be converted.
    bool FindFiles(const Options& options, std::vector<fs::path>& files, std::vector<Pictures>& found)
    {
        for (const fs::path& folder : options.conversion.folders)
        {
            if (!fs::is_directory(folder))
            {
                std::cout << kRed << "error: " << Utf8(fs::absolute(folder)) << " does not exist." << kEnd << "\n";
                return false;
            }
        }

        bool match = true;
        found.resize(options.conversion.folders.size());
        for (std::size_t i = 0; i < options.conversion.folders.size(); ++i)
            match = FindPictures(options.conversion.folders[i], options.recursive, found[i]) && match;
        const Pictures& bottoms = found[0];
        const Pictures& tops = found[1];

        if (bottoms.size() != tops.size())
        {
            const std::size_t difference = bottoms.size() > tops.size() ? bottoms.size() - tops.size() : tops.size() - bottoms.size();
            std::cout << kRed << "error: number of files differs: " << bottoms.size() << " vs " << tops.size() << " (" << difference << ")"
                      << kEnd << "\n";
            match = false;
        }
        match = CheckMissing(options.conversion.folders[1], bottoms, tops) && match;
        match = CheckMissing(options.conversion.folders[0], tops, bottoms) && match;
        if (!match)
        {
            std::cout << kRed << "Bad. Check errors. Conversion cancelled." << kEnd << "\n";
            Pause();
            return false;
        }
        if (bottoms.empty())
        {
            std::cout << kYellow << "warning: no pictures in " << Utf8(fs::absolute(options.conversion.folders[0])) << "." << kEnd << "\n";
            return true;
        }

        bool warned = false;
        for (std::size_t i = 2; i < found.size(); ++i)
        {
            const std::vector<fs::path> only = img2ffpsd::OnlyIn(found[i], bottoms);
            if (only.empty())
                continue;
            std::cout << kYellow << "warning: files missing in '" << Utf8(fs::absolute(options.conversion.folders[0])) << "', skipped in '"
                      << Utf8(fs::absolute(options.conversion.folders[i])) << "':\n";
            for (const fs::path& file : only)
                std::cout << "    - " << Utf8(file) << "\n";
            std::cout << kEnd << "\n";
            warned = true;
        }

        const std::vector<std::string> gaps = img2ffpsd::FindGaps(bottoms);
        if (!gaps.empty())
        {
            std::cout << kYellow << "warning: possible misses:\n";
            for (const std::string& gap : gaps)
                std::cout << "    - " << gap << "\n";
            std::cout << kEnd << "\n";
            warned = true;
        }

        if (warned)
        {
            std::cout << kYellow << "Warnings. But if everything is ok, you can continue." << kEnd << "\n";
            Pause();
        }
        else
        {
            std::cout << kGreen << "Good. Number of files: " << bottoms.size() << ". The files match." << kEnd << "\n";
        }

        if (fs::is_directory(options.conversion.output) && !img2ffpsd::VisibleFiles(options.conversion.output, true).empty())
        {
            std::cout << kYellow << "warning: " << Utf8(fs::absolute(options.conversion.output))
                      << " is not empty. Are you sure? Data may be lost." << kEnd << "\n";
            Pause();
        }

        for (const auto& [name, file] : bottoms)
            files.push_back(name);
        return true;
    }

    // The bar and the errors, from any thread.
    class Progress
    {
    public:
        explicit Progress(std::size_t total)
            : total_(total)
        {
            Draw();
        }

        void Done(const fs::path& file, const std::string& error)
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            ++done_;
            if (!error.empty())
            {
                // The error takes the bar's line; the bar is drawn again below it.
                ++failed_;
                std::cout << "\r\033[K" << kRed << "error: " << Utf8(file) << ": " << error << kEnd << "\n";
            }
            Draw();
        }

        std::size_t Failed() const
        {
            return failed_;
        }

    private:
        static constexpr std::size_t kWidth = 30;

        // Fills by eighths of a cell: UTF-8 of the blocks U+2588 and U+258F to U+2589.
        void Draw() const
        {
            const char* const kFull = "\xE2\x96\x88";
            const char* const kEighths[] = {
                "", "\xE2\x96\x8F", "\xE2\x96\x8E", "\xE2\x96\x8D", "\xE2\x96\x8C", "\xE2\x96\x8B", "\xE2\x96\x8A", "\xE2\x96\x89"};

            const std::size_t eighths = done_ * kWidth * 8 / total_;
            const std::size_t full_cells = eighths / 8;
            const std::size_t rest = eighths % 8;

            std::string bar;
            for (std::size_t i = 0; i < full_cells; ++i)
                bar += kFull;
            bar += kEighths[rest];
            bar.append(kWidth - full_cells - (rest != 0 ? 1 : 0), ' ');

            std::cout << "\r[" << bar << "] " << done_ * 100 / total_ << "%  " << done_ << "/" << total_ << std::flush;
        }

        std::mutex mutex_;
        std::size_t total_ = 0;
        std::size_t done_ = 0;
        std::size_t failed_ = 0;
    };

    // Converts every file; returns the failures.
    std::size_t ConvertAll(const Options& options, const std::vector<Pictures>& found, const std::vector<fs::path>& files)
    {
        Progress progress(files.size());
        img2ffpsd::ConvertAll(options.conversion, found, files, options.jobs, [&](const fs::path& file, const std::string& error) {
            progress.Done(file, error);
        });
        return progress.Failed();
    }

    int Run(const std::vector<std::string>& args)
    {
        EnableConsole();

        if (std::find(args.begin(), args.end(), "--help") != args.end() || std::find(args.begin(), args.end(), "-h") != args.end())
        {
            PrintUsage();
            return 0;
        }

        Options options;
        if (!ParseArguments(args, options))
        {
            PrintUsage();
            return 2;
        }

        std::vector<fs::path> files;
        std::vector<Pictures> found;
        if (!FindFiles(options, files, found))
            return 1;
        if (files.empty())
            return 0;

        const auto started = std::chrono::steady_clock::now();
        const std::size_t failed = ConvertAll(options, found, files);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        std::cout << "\n\n"
                  << (failed == 0 ? kGreen : kRed) << files.size() - failed << " of " << files.size() << " converted in " << std::fixed
                  << std::setprecision(1) << seconds << " s";
        if (failed != 0)
            std::cout << ", " << failed << " failed";
        std::cout << kEnd << "\n";
        return failed == 0 ? 0 : 1;
    }
} // namespace

#if defined(_WIN32)
// wmain takes the arguments as UTF-16, so paths in any script survive.
int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(fs::path(argv[i]).u8string());
    return Run(args);
}
#else
int main(int argc, char** argv)
{
    return Run(std::vector<std::string>(argv + 1, argv + argc));
}
#endif
