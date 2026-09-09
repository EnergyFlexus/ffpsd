#ifndef FFPSD_IMAGE_RESOURCES_HPP_
#define FFPSD_IMAGE_RESOURCES_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace ffpsd
{
    // Resource 1005. Resolutions are in pixels per inch; the units only affect display.
    struct ResolutionInfo
    {
        double horizontal = 72.0;
        std::int16_t horizontal_unit = 1; // 1 pixels per inch, 2 per centimeter
        std::int16_t width_unit = 1;      // 1 in, 2 cm, 3 pt, 4 picas, 5 columns
        double vertical = 72.0;
        std::int16_t vertical_unit = 1;
        std::int16_t height_unit = 1;
    };

    // Resource 1057. Only block version 1 is read past the version field.
    struct VersionInfo
    {
        std::uint32_t version = 1;
        bool has_real_merged_data = true;
        std::string writer_name;
        std::string reader_name;
        std::uint32_t file_version = 1;
    };

    // "Image" means the document here; the data is raw, big endian.
    struct ImageResource
    {
        std::uint16_t id = 0;
        std::string name; // usually empty
        std::vector<std::uint8_t> data;
    };
} // namespace ffpsd

#endif // FFPSD_IMAGE_RESOURCES_HPP_
