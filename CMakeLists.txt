cmake_minimum_required(VERSION 3.23)

project(ffpsd
    VERSION 0.1.3
    DESCRIPTION "ffpsd library"
    LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/ffpsd-out"
        CACHE PATH "Install prefix" FORCE)
endif()

option(FFPSD_BUILD_TOOLS "Build command line tools (img2ffpsd)" ON)
option(FFPSD_WITH_PNG "Build PNG loading, with libpng and zlib-ng from vendor/" ON)
option(FFPSD_BUILD_TESTS "Build the unit tests, with GoogleTest from vendor/" OFF)
option(FFPSD_BUILD_BENCHMARKS "Build the benchmarks, with Google Benchmark from vendor/" OFF)
option(FFPSD_BUILD_EXAMPLES "Build the C and C++ examples in examples/" OFF)

# The CRT follows the library kind: a static library carries none of its own,
# while a DLL with /MT gets a second CRT that frees std types from the wrong heap.
if(BUILD_SHARED_LIBS)
    set(ffpsd_static_runtime_default OFF)
else()
    set(ffpsd_static_runtime_default ON)
endif()
option(FFPSD_MSVC_STATIC_RUNTIME "MSVC: link the C runtime statically (/MT)" ${ffpsd_static_runtime_default})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# MSVC only, ignored by other compilers.
if(FFPSD_MSVC_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Before the warning and LTO settings below, which are for our code only.
set(ffpsd_vendored)
if(FFPSD_WITH_PNG)
    list(APPEND ffpsd_vendored libpng zlib-ng)
endif()
if(FFPSD_BUILD_TESTS)
    list(APPEND ffpsd_vendored googletest)
endif()
if(FFPSD_BUILD_BENCHMARKS)
    list(APPEND ffpsd_vendored benchmark)
endif()
foreach(ffpsd_library IN LISTS ffpsd_vendored)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/vendor/${ffpsd_library}/CMakeLists.txt")
        message(FATAL_ERROR "vendor/${ffpsd_library} is empty: run git submodule update --init")
    endif()
endforeach()
if(ffpsd_vendored)
    add_subdirectory(vendor)
endif()

if(MSVC)
    set(ffpsd_warnings /W4 /permissive-)
    set(ffpsd_optimize /O2 /Ob3 /Oi /Ot /Gy)
    string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
else()
    set(ffpsd_warnings -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    set(ffpsd_cxx_warnings -Wold-style-cast) # C has no other cast
    set(ffpsd_optimize -O3)
endif()

add_compile_options(
    "$<$<CONFIG:Debug>:${ffpsd_warnings}>"
    "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:${ffpsd_cxx_warnings}>"
    "$<$<CONFIG:Release>:${ffpsd_optimize}>")

include(CheckIPOSupported)
check_ipo_supported(RESULT ffpsd_has_lto OUTPUT ffpsd_lto_error)
if(ffpsd_has_lto)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(STATUS "LTO unavailable: ${ffpsd_lto_error}")
endif()

include(GNUInstallDirs)

add_subdirectory(ffpsd)

# img2ffpsd reads PNG files, so it needs the PNG support.
if(FFPSD_BUILD_TOOLS AND FFPSD_WITH_PNG)
    add_subdirectory(img2ffpsd)
elseif(FFPSD_BUILD_TOOLS)
    message(STATUS "img2ffpsd is skipped: it needs FFPSD_WITH_PNG")
endif()

if(FFPSD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FFPSD_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(FFPSD_BUILD_EXAMPLES)
    enable_language(C)
    add_subdirectory(examples/c)
    add_subdirectory(examples/cpp)
endif()

if(BUILD_SHARED_LIBS)
    set(ffpsd_kind shared)
else()
    set(ffpsd_kind static)
endif()

if(MSVC AND BUILD_SHARED_LIBS AND FFPSD_MSVC_STATIC_RUNTIME)
    message(WARNING "ffpsd: a DLL with the static CRT is safe to use through the C ABI only")
endif()

if(MSVC AND FFPSD_MSVC_STATIC_RUNTIME)
    set(ffpsd_crt ", static CRT")
elseif(MSVC)
    set(ffpsd_crt ", dynamic CRT")
endif()
message(STATUS "ffpsd ${PROJECT_VERSION}: ${ffpsd_kind}, C++ API + C ABI${ffpsd_crt}")
