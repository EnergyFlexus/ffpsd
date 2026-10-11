#ifndef IMG2FFPSD_COMMON_CONVERT_HPP_
#define IMG2FFPSD_COMMON_CONVERT_HPP_

#include "common/pictures.hpp"

#include <atomic>
#include <cstddef>
#include <ffpsd/types.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace img2ffpsd
{
    // Auto takes the colors of the top picture, as the size: gray by its file or by every pixel having equal red, green and blue.
    enum class ColorChoice
    {
        kAuto,
        kColor,
        kGray
    };

    // What every file is converted with.
    struct Conversion
    {
        std::vector<std::filesystem::path> folders; // the bottom, the top and the optional ones above it
        std::filesystem::path output;
        ColorChoice color = ColorChoice::kAuto;
        ffpsd::ResampleFilter filter = ffpsd::ResampleFilter::kNearest;
    };

    // Background for the bottom folder, Layer 1 for the top one and so on.
    std::string LayerName(std::size_t folder);

    // One PSD from the pictures of name in the folders that have it, found holding each folder's; resized to the top one.
    void Convert(const Conversion& conversion, const std::vector<Pictures>& found, const std::filesystem::path& name);

    // Called from any thread after each file, with what it threw or nothing.
    using FileDone = std::function<void(const std::filesystem::path& name, const std::string& error)>;

    // Every name, on jobs threads (0: one per processor), each taking the next until none are left or cancel is set.
    void ConvertAll(
        const Conversion& conversion, const std::vector<Pictures>& found, const std::vector<std::filesystem::path>& names, unsigned jobs,
        const FileDone& done, const std::atomic<bool>* cancel = nullptr);
} // namespace img2ffpsd

#endif // IMG2FFPSD_COMMON_CONVERT_HPP_
