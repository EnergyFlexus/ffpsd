cmake_minimum_required(VERSION 3.23)
# CXX too: a static ffpsd is C++ inside, so the link needs the C++ runtime.
project(ffpsd_example_c LANGUAGES C CXX)

# Inside the ffpsd build the target exists; on its own, it comes from an installed package.
if(NOT TARGET ffpsd::ffpsd)
    find_package(ffpsd REQUIRED)
endif()

add_executable(ffpsd_example_c main.c)
target_link_libraries(ffpsd_example_c PRIVATE ffpsd::ffpsd)

# Windows looks for a DLL next to the program first.
get_target_property(ffpsd_type ffpsd::ffpsd TYPE)
if(WIN32 AND ffpsd_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET ffpsd_example_c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ffpsd::ffpsd> $<TARGET_FILE_DIR:ffpsd_example_c>)
endif()
