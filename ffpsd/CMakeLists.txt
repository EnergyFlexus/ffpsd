add_library(ffpsd)
add_library(ffpsd::ffpsd ALIAS ffpsd)

target_sources(ffpsd
    PRIVATE
        src/c_api.cpp
        src/document.cpp
        src/ffpsd.cpp
        src/layer.cpp
        src/detail/color.cpp
        src/detail/color.hpp
        src/detail/color_mode_data.cpp
        src/detail/composite.cpp
        src/detail/composite.hpp
        src/detail/exif.cpp
        src/detail/exif.hpp
        src/detail/color_mode_data.hpp
        src/detail/file_header.cpp
        src/detail/image_data.cpp
        src/detail/image_data.hpp
        src/detail/pixel_data.cpp
        src/detail/pixel_data.hpp
        src/detail/planes.cpp
        src/detail/planes.hpp
        src/detail/resample.cpp
        src/detail/resample.hpp
        src/detail/file_header.hpp
        src/detail/io/big_endian_reader.cpp
        src/detail/io/big_endian_reader.hpp
        src/detail/io/big_endian_writer.cpp
        src/detail/io/big_endian_writer.hpp
        src/detail/io/byte_order.hpp
        src/detail/io/compression.cpp
        src/detail/io/compression.hpp
        src/detail/io/file.cpp
        src/detail/io/file.hpp
        src/detail/io/fixed.hpp
        src/detail/io/fourcc.hpp
        src/detail/io/strings.cpp
        src/detail/io/strings.hpp
        src/detail/layer_and_mask/adjustments/adjustment_layer.cpp
        src/detail/layer_and_mask/adjustments/adjustment_layer.hpp
        src/detail/layer_and_mask/channel_image_data.cpp
        src/detail/layer_and_mask/channel_image_data.hpp
        src/detail/layer_and_mask/global_layer_mask_info.cpp
        src/detail/layer_and_mask/global_layer_mask_info.hpp
        src/detail/layer_and_mask/layer_and_mask_info.cpp
        src/detail/layer_and_mask/layer_and_mask_info.hpp
        src/detail/layer_and_mask/layer_info.cpp
        src/detail/layer_and_mask/layer_info.hpp
        src/detail/layer_and_mask/layer_pixels.cpp
        src/detail/layer_and_mask/layer_pixels.hpp
        src/detail/layer_and_mask/layer_record.cpp
        src/detail/layer_and_mask/adjustments/levels.cpp
        src/detail/layer_and_mask/adjustments/levels.hpp
        src/detail/layer_and_mask/layer_record.hpp
        src/detail/layer_and_mask/tagged_blocks/layer_id.cpp
        src/detail/layer_and_mask/tagged_blocks/layer_id.hpp
        src/detail/layer_and_mask/tagged_blocks/layer_name_source_setting.cpp
        src/detail/layer_and_mask/tagged_blocks/layer_name_source_setting.hpp
        src/detail/layer_and_mask/tagged_blocks/protected_setting.cpp
        src/detail/layer_and_mask/tagged_blocks/protected_setting.hpp
        src/detail/layer_and_mask/tagged_blocks/section_divider_setting.cpp
        src/detail/layer_and_mask/tagged_blocks/section_divider_setting.hpp
        src/detail/layer_and_mask/tagged_blocks/tagged_block.cpp
        src/detail/layer_and_mask/tagged_blocks/tagged_block.hpp
        src/detail/layer_and_mask/tagged_blocks/unicode_layer_name.cpp
        src/detail/layer_and_mask/tagged_blocks/unicode_layer_name.hpp
        src/detail/image_resources/document_specific_ids_seed_number.cpp
        src/detail/image_resources/document_specific_ids_seed_number.hpp
        src/detail/image_resources/image_resource.cpp
        src/detail/image_resources/image_resource.hpp
        src/detail/image_resources/resolution_info.cpp
        src/detail/image_resources/resolution_info.hpp
        src/detail/image_resources/version_info.cpp
        src/detail/image_resources/version_info.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/ffpsd/export.h
            include/ffpsd/ffpsd.hpp
            include/ffpsd/adjustments.hpp
            include/ffpsd/c_api.h
            include/ffpsd/document.hpp
            include/ffpsd/image.hpp
            include/ffpsd/layer.hpp
            include/ffpsd/image_resources.hpp
            include/ffpsd/tagged_block.hpp
            include/ffpsd/types.hpp)

target_include_directories(ffpsd PRIVATE src)

if(FFPSD_WITH_PNG)
    target_sources(ffpsd
        PRIVATE
            src/png.cpp
        PUBLIC
            FILE_SET HEADERS
            FILES
                include/ffpsd/png.hpp)
    target_link_libraries(ffpsd PRIVATE png_static)
    target_compile_definitions(ffpsd PUBLIC FFPSD_HAS_PNG)
endif()

if(FFPSD_WITH_JPEG)
    target_sources(ffpsd
        PRIVATE
            src/jpeg.cpp
        PUBLIC
            FILE_SET HEADERS
            FILES
                include/ffpsd/jpeg.hpp)
    # An imported library cannot join the export set, so a static package names the file it installs.
    get_target_property(ffpsd_jpeg_location ffpsd_jpeg IMPORTED_LOCATION)
    get_filename_component(ffpsd_jpeg_file "${ffpsd_jpeg_location}" NAME)
    target_link_libraries(ffpsd PRIVATE
        "$<BUILD_INTERFACE:ffpsd_jpeg>"
        "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/${ffpsd_jpeg_file}>")
    target_compile_definitions(ffpsd PUBLIC FFPSD_HAS_JPEG)
endif()

# Keeps the symbols of the vendored libraries inside the .so.
if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(ffpsd PRIVATE "LINKER:--exclude-libs,ALL")
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(ffpsd PRIVATE FFPSD_BUILDING)
else()
    target_compile_definitions(ffpsd PUBLIC FFPSD_STATIC)
endif()

target_compile_definitions(ffpsd PRIVATE FFPSD_VERSION="${PROJECT_VERSION}")

set_target_properties(ffpsd PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

# install
include(CMakePackageConfigHelpers)

install(TARGETS ffpsd
    EXPORT ffpsd-targets
    FILE_SET HEADERS)

# Beside the licenses of the vendored libraries.
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME ffpsd.txt)

# A static ffpsd needs them at link time, so they ship in the same package.
if(FFPSD_WITH_PNG AND NOT BUILD_SHARED_LIBS)
    install(TARGETS png_static zlib-ng EXPORT ffpsd-targets)
endif()
if(FFPSD_WITH_JPEG AND NOT BUILD_SHARED_LIBS)
    install(FILES "${ffpsd_jpeg_location}" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

install(EXPORT ffpsd-targets
    FILE ffpsd-targets.cmake
    NAMESPACE ffpsd::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ffpsd")

# The dependencies are in the same export set, so the package config only pulls
# in the exported targets - no template file needed.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config.cmake"
    [[include("${CMAKE_CURRENT_LIST_DIR}/ffpsd-targets.cmake")]])

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config-version.cmake"
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ffpsd")
