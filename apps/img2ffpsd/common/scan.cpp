#include "common/scan.hpp"

#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace img2ffpsd
{
    namespace
    {
        // A folder as the messages name it: its layer and where it is.
        std::string Named(const Conversion& conversion, std::size_t folder)
        {
            return LayerName(folder) + " '" + fs::absolute(conversion.folders[folder]).u8string() + "'";
        }

        std::vector<std::string> Strings(const std::vector<fs::path>& files)
        {
            std::vector<std::string> strings;
            for (const fs::path& file : files)
                strings.push_back(file.u8string());
            return strings;
        }

        void AddMissing(std::vector<Issue>& issues, std::string text, const std::vector<fs::path>& files)
        {
            if (!files.empty())
                issues.push_back(Issue{std::move(text), Strings(files)});
        }
    } // namespace

    FolderScan ScanFolders(const Conversion& conversion, bool recursive)
    {
        FolderScan scan;
        for (std::size_t i = 0; i < conversion.folders.size(); ++i)
        {
            if (!fs::is_directory(conversion.folders[i]))
                scan.errors.push_back(Issue{Named(conversion, i) + " does not exist", {}});
        }
        if (!scan.errors.empty())
            return scan;

        for (std::size_t i = 0; i < conversion.folders.size(); ++i)
        {
            std::vector<std::vector<fs::path>> duplicates;
            scan.found.push_back(FindPictures(conversion.folders[i], recursive, duplicates));
            Issue same_name{"pictures with the same name in " + Named(conversion, i), {}};
            for (const std::vector<fs::path>& files : duplicates)
            {
                std::string names = files[0].u8string();
                for (std::size_t j = 1; j < files.size(); ++j)
                    names += " and " + files[j].u8string();
                same_name.items.push_back(names);
            }
            if (!same_name.items.empty())
                scan.errors.push_back(std::move(same_name));
        }

        const Pictures& bottoms = scan.found[0];
        const Pictures& tops = scan.found[1];
        if (bottoms.empty())
            scan.errors.push_back(Issue{"no pictures in " + Named(conversion, 0), {}});
        if (bottoms.size() != tops.size())
            scan.errors.push_back(
                Issue{
                    "number of files differs: " + std::to_string(bottoms.size()) + " in " + LayerName(0) + ", " +
                        std::to_string(tops.size()) + " in " + LayerName(1),
                    {}});
        AddMissing(scan.errors, "files missing in " + Named(conversion, 1), OnlyIn(bottoms, tops));
        AddMissing(scan.errors, "files missing in " + Named(conversion, 0), OnlyIn(tops, bottoms));

        for (std::size_t i = 2; i < scan.found.size(); ++i)
            AddMissing(
                scan.warnings, "files missing in " + LayerName(0) + ", skipped in " + Named(conversion, i), OnlyIn(scan.found[i], bottoms));
        const std::vector<std::string> gaps = FindGaps(bottoms);
        if (!gaps.empty())
            scan.warnings.push_back(Issue{"possible misses", gaps});
        if (fs::is_directory(conversion.output) && !VisibleFiles(conversion.output, true).empty())
            scan.warnings.push_back(
                Issue{"output '" + fs::absolute(conversion.output).u8string() + "' is not empty: files there may be replaced", {}});

        for (const auto& [name, file] : bottoms)
            scan.names.push_back(name);
        return scan;
    }
} // namespace img2ffpsd
