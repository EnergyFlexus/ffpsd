#ifndef IMG2FFPSD_COMMON_SCAN_HPP_
#define IMG2FFPSD_COMMON_SCAN_HPP_

#include "common/convert.hpp"
#include "common/pictures.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace img2ffpsd
{
    // One finding: what is wrong, and the files or pages it is about, if it names any.
    struct Issue
    {
        std::string text;
        std::vector<std::string> items;
    };

    // What the folders hold: errors stop the conversion, warnings want a look before it.
    struct FolderScan
    {
        std::vector<Pictures> found;              // per folder of the conversion
        std::vector<std::filesystem::path> names; // the bottom's, to convert
        std::vector<Issue> errors;
        std::vector<Issue> warnings;
    };

    // The folders against each other and the output, as both apps check them before converting.
    FolderScan ScanFolders(const Conversion& conversion, bool recursive);
} // namespace img2ffpsd

#endif // IMG2FFPSD_COMMON_SCAN_HPP_
