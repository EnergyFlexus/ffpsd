#include "common/pictures.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ffpsd/formats.hpp>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace img2ffpsd
{
    namespace
    {
        std::string Extension(const fs::path& path)
        {
            std::string extension = path.extension().u8string();
            for (char& c : extension)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return extension;
        }

        bool IsPicture(const fs::path& path)
        {
            const std::string extension = Extension(path);
            if (extension == ".webp")
                return ffpsd::IsFormatSupported(ffpsd::Format::kWebp);
            return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
        }

        // What the system leaves behind: Finder's .DS_Store and ._ files, which are no pictures, Thumbs.db and the like.
        bool IsHidden(const fs::path& path)
        {
            const std::string name = path.filename().u8string();
            return name.empty() || name[0] == '.' || name == "Thumbs.db" || name == "desktop.ini";
        }

        // Color type 0 is gray and 4 gray with alpha; it is the byte after the bit depth in IHDR, which comes first.
        bool IsGrayPng(std::istream& in)
        {
            std::array<char, 26> header{};
            return in.read(header.data(), header.size()) && (header[25] == 0 || header[25] == 4);
        }

        // The component count of the start of frame, which comes before the scan.
        bool IsGrayJpeg(std::istream& in)
        {
            const auto next = [&in]() { return in.get(); };
            in.ignore(2); // the start of image
            while (in)
            {
                int marker = next();
                if (marker != 0xFF)
                    return false;
                while (marker == 0xFF)
                    marker = next(); // fill bytes
                if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8))
                    continue; // no length
                if (marker == 0xD9 || marker == 0xDA || marker < 0)
                    return false; // the end, or the scan without a frame first

                const int high = next();
                const int length = high << 8 | next();
                if (length < 2)
                    return false;
                const bool frame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
                if (frame)
                {
                    in.ignore(5); // precision, height and width
                    return next() == 1;
                }
                in.ignore(length - 2);
            }
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
    } // namespace

    std::vector<fs::path> VisibleFiles(const fs::path& folder, bool recursive)
    {
        std::vector<fs::path> files;
        for (auto entry = fs::recursive_directory_iterator(folder); entry != fs::recursive_directory_iterator(); ++entry)
        {
            if (entry->is_directory())
            {
                if (!recursive || IsHidden(entry->path()))
                    entry.disable_recursion_pending();
            }
            else if (!IsHidden(entry->path()) && entry->is_regular_file())
            {
                files.push_back(entry->path());
            }
        }
        return files;
    }

    Pictures FindPictures(const fs::path& folder, bool recursive, std::vector<std::vector<fs::path>>& duplicates)
    {
        std::map<fs::path, std::vector<fs::path>> by_name;
        for (const fs::path& file : VisibleFiles(folder, recursive))
        {
            if (!IsPicture(file))
                continue;
            const fs::path relative = file.lexically_relative(folder);
            by_name[fs::path(relative).replace_extension()].push_back(relative);
        }

        Pictures pictures;
        for (auto& [name, files] : by_name)
        {
            if (files.size() == 1)
                pictures[name] = files[0];
            else
                duplicates.push_back(std::move(files));
        }
        return pictures;
    }

    bool IsGrayPicture(const fs::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        const int first = in.peek();
        if (first == 0x89)
            return IsGrayPng(in);
        if (first == 0xFF)
            return IsGrayJpeg(in);
        return false;
    }

    std::vector<fs::path> OnlyIn(const Pictures& pictures, const Pictures& other)
    {
        std::vector<fs::path> only;
        for (const auto& [name, file] : pictures)
        {
            if (other.count(name) == 0)
                only.push_back(file);
        }
        return only;
    }

    std::vector<std::string> FindGaps(const Pictures& pictures)
    {
        std::map<fs::path, std::set<long long>> pages_by_folder;
        for (const auto& [name, file] : pictures)
            AddPages(name.filename().u8string(), pages_by_folder[name.parent_path()]);

        std::vector<std::string> gaps;
        for (const auto& [folder, pages] : pages_by_folder)
        {
            if (pages.empty())
                continue;
            for (long long page = *pages.begin(); page < *pages.rbegin(); ++page)
            {
                if (pages.count(page) == 0)
                    gaps.push_back((folder / std::to_string(page)).u8string());
            }
        }
        return gaps;
    }
} // namespace img2ffpsd
