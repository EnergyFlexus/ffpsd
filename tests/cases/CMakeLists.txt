# A folder per public header, and smoke/ for real files end to end.
# c_header_check.c is compiled as C, which is how the C header is proven to be C.
enable_language(C)

add_executable(ffpsd_tests
    adjustments/levels_test.cpp
    c_api/c_api_test.cpp
    c_api/c_header_check.c
    document/canvas_test.cpp
    document/color_mode_test.cpp
    document/compression_test.cpp
    document/document_test.cpp
    document/file_test.cpp
    document/stack_test.cpp
    formats/formats_test.cpp
    layer/background_test.cpp
    layer/layer_test.cpp
    layer/mask_test.cpp
    layer/pixels_test.cpp
    layer/transform_test.cpp
    smoke/read_psd_test.cpp
    smoke/rgb_levels_test.cpp
    support/test_support.hpp)

if(FFPSD_WITH_PNG)
    target_sources(ffpsd_tests PRIVATE formats/png_test.cpp)
endif()
if(FFPSD_WITH_JPEG)
    target_sources(ffpsd_tests PRIVATE formats/jpeg_test.cpp)
endif()
if(FFPSD_WITH_WEBP)
    target_sources(ffpsd_tests PRIVATE formats/webp_test.cpp)
endif()

target_include_directories(ffpsd_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ffpsd_tests PRIVATE FFPSD_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
target_link_libraries(ffpsd_tests PRIVATE ffpsd::ffpsd GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(ffpsd_tests)

# A static ffpsd holds the image libraries; a symbol of theirs without the prefix would clash with the program's own copy.
if(NOT BUILD_SHARED_LIBS AND (FFPSD_WITH_PNG OR FFPSD_WITH_JPEG OR FFPSD_WITH_WEBP))
    ffpsd_symbols_command(ffpsd_command check "-DARCHIVE=$<TARGET_FILE:ffpsd>")
    add_test(NAME vendored_symbols COMMAND ${ffpsd_command})
endif()
