#ifndef IMG2FFPSD_COMMON_SCAN_HPP_
#define IMG2FFPSD_COMMON_SCAN_HPP_

#include "common/convert.hpp"
#include "common/pictures.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace img2ffpsd
{
    struct Issue
    {
        std::string text;
        std::vector<std::string> items;
    };

    // Errors stop the conversion; warnings only want a look.
    struct FolderScan
    {
        std::vector<Pictures> found;              // per folder of the conversion
        std::vector<std::filesystem::path> names; // the bottom's, to convert
        std::vector<Issue> errors;
        std::vector<Issue> warnings;
    };

    // Checks the folders against each other and the output.
    FolderScan ScanFolders(const Conversion& conversion, bool recursive);
} // namespace img2ffpsd

#endif // IMG2FFPSD_COMMON_SCAN_HPP_
