# Third-party code, always static: a DLL carries it inside, a static ffpsd ships
# it next to itself. The CRT setting comes from the top level.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(SKIP_INSTALL_ALL ON)

include(GNUInstallDirs)

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
    install(FILES libpng/LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME libpng.txt)
    install(FILES zlib-ng/LICENSE.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME zlib-ng.md)
endif()

if(FFPSD_WITH_JPEG)
    # libjpeg-turbo refuses add_subdirectory, so it is built on its own and installed into the build tree.
    include(ExternalProject)
    set(ffpsd_jpeg_dir "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-install")
    if(MSVC)
        set(ffpsd_jpeg_file jpeg-static.lib)
    else()
        set(ffpsd_jpeg_file libjpeg.a)
    endif()
    if(FFPSD_MSVC_STATIC_RUNTIME)
        set(ffpsd_jpeg_crt_dll 0)
    else()
        set(ffpsd_jpeg_crt_dll 1)
    endif()

    # Plain C, without SIMD: no assembler to install on any system.
    set(ffpsd_jpeg_args
        -DCMAKE_INSTALL_PREFIX=${ffpsd_jpeg_dir}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_INSTALL_INCLUDEDIR=include
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DENABLE_SHARED=0
        -DENABLE_STATIC=1
        -DWITH_TURBOJPEG=0
        -DWITH_TOOLS=0
        -DWITH_TESTS=0
        -DWITH_SIMD=0
        -DWITH_CRT_DLL=${ffpsd_jpeg_crt_dll})
    foreach(ffpsd_variable IN ITEMS CMAKE_BUILD_TYPE CMAKE_C_COMPILER CMAKE_MAKE_PROGRAM CMAKE_TOOLCHAIN_FILE
                                    CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET)
        if(${ffpsd_variable})
            list(APPEND ffpsd_jpeg_args "-D${ffpsd_variable}=${${ffpsd_variable}}")
        endif()
    endforeach()

    ExternalProject_Add(libjpeg-turbo
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg-turbo"
        CMAKE_ARGS ${ffpsd_jpeg_args}
        BUILD_BYPRODUCTS "${ffpsd_jpeg_dir}/lib/${ffpsd_jpeg_file}"
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        LOG_OUTPUT_ON_FAILURE ON)

    # The include directory has to exist when the imported target is made, long before the build fills it.
    file(MAKE_DIRECTORY "${ffpsd_jpeg_dir}/include")
    add_library(ffpsd_jpeg STATIC IMPORTED GLOBAL)
    set_target_properties(ffpsd_jpeg PROPERTIES
        IMPORTED_LOCATION "${ffpsd_jpeg_dir}/lib/${ffpsd_jpeg_file}"
        INTERFACE_INCLUDE_DIRECTORIES "${ffpsd_jpeg_dir}/include")
    add_dependencies(ffpsd_jpeg libjpeg-turbo)

    install(FILES libjpeg-turbo/LICENSE.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME libjpeg-turbo.md)
    install(FILES libjpeg-turbo/README.ijg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ffpsd/licenses" RENAME libjpeg-turbo-ijg.txt)
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
