# Third-party code, always static: a DLL carries it inside, a static ffpsd ships
# it next to itself. The CRT setting comes from the top level.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(SKIP_INSTALL_ALL ON)

if(FFPSD_WITH_PNG)
    # zlib-ng with the zlib API, which is what libpng is written against.
    set(ZLIB_COMPAT ON)
    set(WITH_GTEST OFF)
    set(WITH_GZFILEOP OFF)
    add_subdirectory(zlib-ng)

    # libpng calls find_package(ZLIB); the cache entries and the alias make it take
    # zlib-ng instead of searching the system.
    set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/zlib-ng" CACHE PATH "zlib-ng headers" FORCE)
    set(ZLIB_LIBRARY zlib-ng CACHE STRING "zlib-ng target" FORCE)
    add_library(ZLIB::ZLIB ALIAS zlib-ng)

    set(PNG_SHARED OFF)
    set(PNG_STATIC ON)
    set(PNG_TESTS OFF)
    set(PNG_TOOLS OFF)
    set(PNG_FRAMEWORK OFF)
    add_subdirectory(libpng)

    # ffpsd includes png.h privately; an installed consumer needs the library, not the headers.
    set_property(TARGET png_static PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libpng;${CMAKE_CURRENT_BINARY_DIR}/libpng>")

    # Their licenses travel with the binaries that contain their code.
    include(GNUInstallDirs)
    install(FILES libpng/LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME libpng.txt)
    install(FILES zlib-ng/LICENSE.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME zlib-ng.md)
endif()

# Only the tests link it, and nothing of it is installed.
if(FFPSD_BUILD_TESTS)
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
    add_subdirectory(googletest)
endif()

# Only the benchmarks link it; its own tests and -Werror stay off.
if(FFPSD_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_ENABLE_WERROR OFF)
    add_subdirectory(benchmark)
endif()
