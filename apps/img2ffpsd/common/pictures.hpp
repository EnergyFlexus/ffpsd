#ifndef IMG2FFPSD_COMMON_PICTURES_HPP_
#define IMG2FFPSD_COMMON_PICTURES_HPP_

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace img2ffpsd
{
    // A folder's pictures: the relative path without the extension, which pairs them, to the file itself.
    using Pictures = std::map<std::filesystem::path, std::filesystem::path>;

    // The regular files under the folder, or in it alone unless recursive; hidden files and whatever hidden folders hold left out.
    std::vector<std::filesystem::path> VisibleFiles(const std::filesystem::path& folder, bool recursive);

    // The PNG and JPEG files of VisibleFiles; a name that more than one has, 01.png and 01.jpg, goes into duplicates instead.
    Pictures FindPictures(const std::filesystem::path& folder, bool recursive, std::vector<std::vector<std::filesystem::path>>& duplicates);

    // A PNG of gray or gray with alpha, or a JPEG of one component, by the file's header alone; false for anything else.
    bool IsGrayPicture(const std::filesystem::path& file);

    // The pictures whose name the other folder lacks.
    std::vector<std::filesystem::path> OnlyIn(const Pictures& pictures, const Pictures& other);

    // Pages missing between the first and the last page of each folder: 12 is page 12, 12-13 pages 12 and 13.
    std::vector<std::string> FindGaps(const Pictures& pictures);
} // namespace img2ffpsd

#endif // IMG2FFPSD_COMMON_PICTURES_HPP_
