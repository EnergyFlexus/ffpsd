add_library(ffpsd)
add_library(ffpsd::ffpsd ALIAS ffpsd)

target_sources(ffpsd
    PRIVATE
        src/bytes.cpp
        src/c_api.cpp
        src/document.cpp
        src/ffpsd.cpp
        src/formats.cpp
        src/layer.cpp
        src/detail/color.cpp
        src/detail/color.hpp
        src/detail/color_mode_data.cpp
        src/detail/composite.cpp
        src/detail/composite.hpp
        src/detail/color_mode_data.hpp
        src/detail/file_header.cpp
        src/detail/image_data.cpp
        src/detail/image_data.hpp
        src/detail/image.cpp
        src/detail/image.hpp
        src/detail/limits.cpp
        src/detail/limits.hpp
        src/detail/pixel_data.cpp
        src/detail/pixel_data.hpp
        src/detail/rect.cpp
        src/detail/rect.hpp
        src/detail/resample.cpp
        src/detail/resample.hpp
        src/detail/samples.hpp
        src/detail/signatures.hpp
        src/detail/transform.cpp
        src/detail/transform.hpp
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
        src/detail/layer_and_mask/channel_ids.hpp
        src/detail/layer_and_mask/channel_image_data.cpp
        src/detail/layer_and_mask/channel_image_data.hpp
        src/detail/layer_and_mask/global_layer_mask_info.cpp
        src/detail/layer_and_mask/global_layer_mask_info.hpp
        src/detail/layer_and_mask/layer_and_mask_info.cpp
        src/detail/layer_and_mask/layer_and_mask_info.hpp
        src/detail/layer_and_mask/layer_geometry.cpp
        src/detail/layer_and_mask/layer_geometry.hpp
        src/detail/layer_and_mask/layer_info.cpp
        src/detail/layer_and_mask/layer_info.hpp
        src/detail/layer_and_mask/layer_mask.cpp
        src/detail/layer_and_mask/layer_mask.hpp
        src/detail/layer_and_mask/layer_pixels.cpp
        src/detail/layer_and_mask/layer_pixels.hpp
        src/detail/layer_and_mask/layer_record.cpp
        src/detail/layer_and_mask/layer_record.hpp
        src/detail/layer_and_mask/mask_data.cpp
        src/detail/layer_and_mask/mask_data.hpp
        src/detail/layer_and_mask/adjustments/levels.cpp
        src/detail/layer_and_mask/adjustments/levels.hpp
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
        src/detail/formats/exif.cpp
        src/detail/formats/exif.hpp
        src/detail/formats/picture.cpp
        src/detail/formats/picture.hpp
        src/detail/formats/planes.cpp
        src/detail/formats/planes.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/ffpsd/export.h
            include/ffpsd/ffpsd.hpp
            include/ffpsd/adjustments.hpp
            include/ffpsd/bytes.hpp
            include/ffpsd/c_api.h
            include/ffpsd/document.hpp
            include/ffpsd/formats.hpp
            include/ffpsd/image.hpp
            include/ffpsd/layer.hpp
            include/ffpsd/image_resources.hpp
            include/ffpsd/tagged_block.hpp
            include/ffpsd/types.hpp)

target_include_directories(ffpsd PRIVATE src)

# LTO for a shared library only: a static one hands its objects to the program's linker, which may not read
# LTO objects. PIC lets a static ffpsd go into a shared library too, such as a Rust cdylib.
if(BUILD_SHARED_LIBS AND ffpsd_has_lto)
    set_property(TARGET ffpsd PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
set_property(TARGET ffpsd PROPERTY POSITION_INDEPENDENT_CODE ON)

# The public headers use std::optional, so a program needs C++17 too; the package passes it on.
target_compile_features(ffpsd PUBLIC cxx_std_17)

# The image libraries end up inside ffpsd: a shared one links them, a static one takes only their headers here
# and their objects below, so nothing, in the build tree or installed, links them beside it.
# The file that calls a library needs the renames it was built with.
set(ffpsd_vendored_archives)
if(NOT BUILD_SHARED_LIBS)
    # Through a target of its own: Visual Studio would otherwise archive a static library's dependencies into it too.
    add_custom_target(ffpsd_vendored_built)
    add_dependencies(ffpsd ffpsd_vendored_built)
endif()
function(ffpsd_use_vendored source header)
    ffpsd_prefix_option(option "${PROJECT_SOURCE_DIR}/cmake/prefix/${header}")
    set_source_files_properties(${source} PROPERTIES COMPILE_OPTIONS "${option}")
    set(archives ${ffpsd_vendored_archives})
    foreach(library IN LISTS ARGN)
        if(BUILD_SHARED_LIBS)
            target_link_libraries(ffpsd PRIVATE ${library})
        else()
            target_include_directories(ffpsd PRIVATE "$<TARGET_PROPERTY:${library},INTERFACE_INCLUDE_DIRECTORIES>")
            add_dependencies(ffpsd_vendored_built ${library})
            list(APPEND archives "$<TARGET_FILE:${library}>")
        endif()
    endforeach()
    set(ffpsd_vendored_archives ${archives} PARENT_SCOPE)
endfunction()

if(FFPSD_WITH_PNG)
    target_sources(ffpsd PRIVATE src/detail/formats/png.cpp src/detail/formats/png.hpp)
    ffpsd_use_vendored(src/detail/formats/png.cpp libpng.h png_static zlib-ng)
    target_compile_definitions(ffpsd PUBLIC FFPSD_HAS_PNG)
endif()

if(FFPSD_WITH_JPEG)
    target_sources(ffpsd PRIVATE src/detail/formats/jpeg.cpp src/detail/formats/jpeg.hpp)
    ffpsd_use_vendored(src/detail/formats/jpeg.cpp libjpeg_turbo.h ffpsd_jpeg)
    target_compile_definitions(ffpsd PUBLIC FFPSD_HAS_JPEG)
endif()

if(FFPSD_WITH_WEBP)
    target_sources(ffpsd PRIVATE src/detail/formats/webp.cpp src/detail/formats/webp.hpp)
    ffpsd_use_vendored(src/detail/formats/webp.cpp libwebp.h ffpsd_webp ffpsd_sharpyuv)
    target_compile_definitions(ffpsd PUBLIC FFPSD_HAS_WEBP)
endif()

# A static library cannot link others, so their objects are added to it after it is archived.
if(NOT BUILD_SHARED_LIBS AND ffpsd_vendored_archives)
    if(MSVC)
        set(ffpsd_merge_args -DSTYLE=msvc "-DTOOL=${CMAKE_LINKER}")
    elseif(APPLE)
        get_filename_component(ffpsd_tools "${CMAKE_AR}" DIRECTORY)
        find_program(FFPSD_LIBTOOL libtool HINTS "${ffpsd_tools}" REQUIRED)
        set(ffpsd_merge_args -DSTYLE=apple "-DTOOL=${FFPSD_LIBTOOL}")
    elseif(CMAKE_CXX_COMPILER_AR)
        # gcc-ar or llvm-ar, which index the LTO objects of a Release build.
        set(ffpsd_merge_args -DSTYLE=gnu "-DTOOL=${CMAKE_CXX_COMPILER_AR}")
    else()
        set(ffpsd_merge_args -DSTYLE=gnu "-DTOOL=${CMAKE_AR}")
    endif()
    # A list on the command line would meet the quoting of cmd.exe, so it goes through a file.
    set(ffpsd_archives "${CMAKE_CURRENT_BINARY_DIR}/vendored_archives_$<CONFIG>.txt")
    file(GENERATE OUTPUT "${ffpsd_archives}" CONTENT "$<JOIN:${ffpsd_vendored_archives},\n>\n")
    add_custom_command(TARGET ffpsd POST_BUILD
        COMMAND "${CMAKE_COMMAND}" ${ffpsd_merge_args} "-DLIBRARY=$<TARGET_FILE:ffpsd>" "-DARCHIVES=${ffpsd_archives}"
                -P "${PROJECT_SOURCE_DIR}/cmake/merge_archives.cmake"
        VERBATIM)
    # Otherwise a library rebuilt alone would leave its old objects inside ffpsd.
    set_property(TARGET ffpsd APPEND PROPERTY LINK_DEPENDS ${ffpsd_vendored_archives})
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

install(EXPORT ffpsd-targets
    FILE ffpsd-targets.cmake
    NAMESPACE ffpsd::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ffpsd")

# ffpsd has no dependencies to find, so the package config only pulls in the
# exported target - no template file needed.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config.cmake"
    [[include("${CMAKE_CURRENT_LIST_DIR}/ffpsd-targets.cmake")]])

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config-version.cmake"
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ffpsd-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ffpsd")
