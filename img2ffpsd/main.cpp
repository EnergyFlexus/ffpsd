// Two folders of PNG pairs into PSD files: the bottom as the background, the top as a layer.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
    struct Options
    {
        fs::path bottom;
        fs::path top;
        fs::path output;
        bool gray = false;
        unsigned jobs = 0; // 0: one per processor
        ffpsd::ResampleFilter resize = ffpsd::ResampleFilter::kNearest;
    };

    // ---- Console -------------------------------------------------------------------------------

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
        std::cout << "img2ffpsd " << ffpsd::Version() << "\n"
                  << "usage: img2ffpsd <bottom> <top> <output> [--gray] [--jobs N] [--resize nearest|bicubic]\n\n"
                  << "Pairs PNG files by their path in the bottom and top folders into PSD files in\n"
                  << "output: the bottom one as the locked background, the top one as the layer above\n"
                  << "it and the composite. The smaller picture of a pair is resized to the larger one.\n\n"
                  << "  --gray              grayscale documents instead of RGB\n"
                  << "  --jobs N            files converted at once, one per processor by default\n"
                  << "  --resize nearest    resizing that keeps hard pixels, the default\n"
                  << "  --resize bicubic    smooth resizing\n";
    }

    bool ParseArguments(const std::vector<std::string>& args, Options& options)
    {
        std::vector<std::string> folders;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--gray")
                options.gray = true;
            else if (args[i] == "--jobs" && i + 1 < args.size())
                options.jobs = static_cast<unsigned>(std::max(1, std::atoi(args[++i].c_str())));
            else if (args[i] == "--resize" && i + 1 < args.size())
            {
                const std::string& method = args[++i];
                if (method == "nearest")
                    options.resize = ffpsd::ResampleFilter::kNearest;
                else if (method == "bicubic")
                    options.resize = ffpsd::ResampleFilter::kBicubic;
                else
                    return false;
            }
            else if (args[i].rfind("--", 0) == 0)
                return false; // an unknown option
            else
                folders.push_back(args[i]);
        }
        if (folders.size() != 3)
            return false;

        options.bottom = fs::u8path(folders[0]);
        options.top = fs::u8path(folders[1]);
        options.output = fs::u8path(folders[2]);
        return true;
    }

    // ---- Files ---------------------------------------------------------------------------------

    std::vector<std::uint8_t> ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("cannot open " + Utf8(path));
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void WriteFile(const fs::path& path, const std::vector<std::uint8_t>& data)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file)
            throw std::runtime_error("cannot write " + Utf8(path));
    }

    bool IsPng(const fs::path& path)
    {
        std::string extension = path.extension().u8string();
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return extension == ".png";
    }

    // Paths relative to the folder, sorted.
    std::set<fs::path> FindPngs(const fs::path& folder)
    {
        std::set<fs::path> files;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(folder))
        {
            if (entry.is_regular_file() && IsPng(entry.path()))
                files.insert(entry.path().lexically_relative(folder));
        }
        return files;
    }

    // ---- Checking the folders ------------------------------------------------------------------

    // Prints the files of one folder that the other lacks; true when there are none.
    bool CheckOnlyIn(const fs::path& folder, const std::set<fs::path>& files, const std::set<fs::path>& other)
    {
        std::vector<fs::path> only;
        std::set_difference(files.begin(), files.end(), other.begin(), other.end(), std::back_inserter(only));
        if (only.empty())
            return true;

        std::cout << kRed << "[!] Files ONLY in '" << Utf8(fs::absolute(folder)) << "':\n";
        for (const fs::path& file : only)
            std::cout << "    - " << Utf8(file) << "\n";
        std::cout << kEnd << "\n";
        return false;
    }

    bool IsNumber(const std::string& text)
    {
        return !text.empty() && text.size() < 18 &&
               std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    }

    // A file named 12 is page 12; one named 12-13 is pages 12 and 13; any other name is no page.
    void AddPages(const std::string& name, std::set<long long>& pages)
    {
        const std::size_t dash = name.find('-');
        if (IsNumber(name))
        {
            pages.insert(std::stoll(name));
        }
        else if (dash != std::string::npos && IsNumber(name.substr(0, dash)) && IsNumber(name.substr(dash + 1)))
        {
            pages.insert(std::stoll(name.substr(0, dash)));
            pages.insert(std::stoll(name.substr(dash + 1)));
        }
    }

    // Pages missing between the first and the last page of each folder.
    std::vector<std::string> FindGaps(const std::set<fs::path>& files)
    {
        std::map<fs::path, std::set<long long>> pages_by_folder;
        for (const fs::path& file : files)
            AddPages(file.stem().u8string(), pages_by_folder[file.parent_path()]);

        std::vector<std::string> gaps;
        for (const auto& [folder, pages] : pages_by_folder)
        {
            if (pages.empty())
                continue;
            for (long long page = *pages.begin(); page < *pages.rbegin(); ++page)
            {
                if (pages.count(page) == 0)
                    gaps.push_back(Utf8(folder / std::to_string(page)));
            }
        }
        return gaps;
    }

    // The paths both folders have; false, after printing why, when they cannot be converted.
    bool FindPairs(const Options& options, std::vector<fs::path>& files)
    {
        for (const fs::path& folder : {options.bottom, options.top})
        {
            if (!fs::is_directory(folder))
            {
                std::cout << kRed << "[!] " << Utf8(fs::absolute(folder)) << " does not exist." << kEnd << "\n";
                return false;
            }
        }

        const std::set<fs::path> bottoms = FindPngs(options.bottom);
        const std::set<fs::path> tops = FindPngs(options.top);

        bool match = true;
        if (bottoms.size() != tops.size())
        {
            const std::size_t difference =
                bottoms.size() > tops.size() ? bottoms.size() - tops.size() : tops.size() - bottoms.size();
            std::cout << kRed << "[!] Number of files difference: " << bottoms.size() << " vs " << tops.size()
                      << " files. (" << difference << ")" << kEnd << "\n";
            match = false;
        }
        match = CheckOnlyIn(options.bottom, bottoms, tops) && match;
        match = CheckOnlyIn(options.top, tops, bottoms) && match;
        if (!match)
        {
            std::cout << kRed << "Bad. Check errors. Conversion cancelled." << kEnd << "\n";
            Pause();
            return false;
        }
        if (bottoms.empty())
        {
            std::cout << kYellow << "[!] No PNG files in " << Utf8(fs::absolute(options.bottom)) << "." << kEnd << "\n";
            return true;
        }

        const std::vector<std::string> gaps = FindGaps(bottoms);
        if (gaps.empty())
        {
            std::cout << kGreen << "Good. Number of files: " << bottoms.size() << ". The files match." << kEnd << "\n";
        }
        else
        {
            std::cout << kYellow << "[!] Possible misses:\n";
            for (const std::string& gap : gaps)
                std::cout << "    - " << gap << "\n";
            std::cout << kEnd << "\n"
                      << kYellow << "Warnings. But if everything is ok, you can continue." << kEnd << "\n";
            Pause();
        }

        if (fs::is_directory(options.output) && !fs::is_empty(options.output))
        {
            std::cout << kYellow << "[!] " << Utf8(fs::absolute(options.output))
                      << " is not empty. Are you sure? Data may be lost." << kEnd << "\n";
            Pause();
        }

        files.assign(bottoms.begin(), bottoms.end());
        return true;
    }

    // ---- Converting one pair -------------------------------------------------------------------

    ffpsd::Image LoadPng(const fs::path& path, ffpsd::ColorMode color)
    {
        const std::vector<std::uint8_t> data = ReadFile(path);
        return ffpsd::LoadPng(data.data(), data.size(), color, 8);
    }

    void Convert(const Options& options, const fs::path& file)
    {
        const ffpsd::ColorMode color = options.gray ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        const std::uint16_t color_count = options.gray ? 1 : 3;

        const ffpsd::Image bottom = LoadPng(options.bottom / file, color);
        ffpsd::Image top = LoadPng(options.top / file, color);

        // The top picture loses its transparency, the plane after the color ones.
        top.channel_count = color_count;
        top.bytes.resize(top.GetSizeBytes());

        // The document is as large as the larger picture.
        const bool top_is_larger = std::uint64_t{top.width} * top.height >= std::uint64_t{bottom.width} * bottom.height;
        const std::uint32_t width = top_is_larger ? top.width : bottom.width;
        const std::uint32_t height = top_is_larger ? top.height : bottom.height;

        ffpsd::Document doc;
        doc.SetColor(color);
        doc.SetWidth(width);
        doc.SetHeight(height);
        doc.SetChannelCount(color_count);

        if (bottom.width == width && bottom.height == height)
        {
            doc.AddBackgroundLayer("Background", bottom);
        }
        else
        {
            doc.AddLayer("Background", bottom)->Resize(width, height, options.resize);
            doc.SetBackgroundLayer(0);
        }

        // The composite is the top picture; only a resized one has to be read back from its layer.
        ffpsd::Layer* layer = doc.AddLayer("Layer 1", top);
        if (top.width == width && top.height == height)
        {
            doc.SetMergedImage(top);
        }
        else
        {
            layer->Resize(width, height, options.resize);
            doc.SetMergedImage(layer->GetPixels());
        }

        fs::path target = options.output / file;
        target.replace_extension(".psd");
        WriteFile(target, doc.Save());
    }

    // ---- Progress ------------------------------------------------------------------------------

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
                std::cout << "\r\033[K" << kRed << "[!] Error " << Utf8(file) << ": " << error << kEnd << "\n";
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
            const char* const kEighths[] = {"",
                                            "\xE2\x96\x8F",
                                            "\xE2\x96\x8E",
                                            "\xE2\x96\x8D",
                                            "\xE2\x96\x8C",
                                            "\xE2\x96\x8B",
                                            "\xE2\x96\x8A",
                                            "\xE2\x96\x89"};

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

    // Converts every pair; each thread takes the next file until none are left. Returns the failures.
    std::size_t ConvertAll(const Options& options, const std::vector<fs::path>& files)
    {
        Progress progress(files.size());
        std::atomic<std::size_t> next{0};

        const auto work = [&] {
            for (std::size_t i = next++; i < files.size(); i = next++)
            {
                std::string error;
                try
                {
                    Convert(options, files[i]);
                }
                catch (const std::exception& e)
                {
                    error = e.what();
                }
                progress.Done(files[i], error);
            }
        };

        const unsigned processors = std::max(1u, std::thread::hardware_concurrency());
        const std::size_t threads = std::min<std::size_t>(options.jobs != 0 ? options.jobs : processors, files.size());
        std::vector<std::thread> workers;
        for (std::size_t i = 1; i < threads; ++i)
            workers.emplace_back(work);
        work(); // the main thread is one of them
        for (std::thread& worker : workers)
            worker.join();

        return progress.Failed();
    }

    // ---- The program ---------------------------------------------------------------------------

    int Run(const std::vector<std::string>& args)
    {
        EnableConsole();

        if (std::find(args.begin(), args.end(), "--help") != args.end() ||
            std::find(args.begin(), args.end(), "-h") != args.end())
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
        if (!FindPairs(options, files))
            return 1;
        if (files.empty())
            return 0;

        const auto started = std::chrono::steady_clock::now();
        const std::size_t failed = ConvertAll(options, files);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        std::cout << "\n\n"
                  << (failed == 0 ? kGreen : kRed) << files.size() - failed << " of " << files.size()
                  << " converted in " << std::fixed << std::setprecision(1) << seconds << " s";
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
