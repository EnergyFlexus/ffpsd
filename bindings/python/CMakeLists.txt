# The Python module, built by scikit-build-core from pyproject.toml at the root; ffpsd goes inside it, static.
cmake_minimum_required(VERSION 3.23)

project(ffpsd_python LANGUAGES CXX)

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)

set(BUILD_SHARED_LIBS OFF)
set(FFPSD_BUILD_APPS OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." ffpsd EXCLUDE_FROM_ALL)

# The stable ABI of 3.12 where the interpreter has it, a module per version below.
nanobind_add_module(_ffpsd STABLE_ABI NB_STATIC
    src/module.cpp
    src/common.cpp
    src/types.cpp
    src/document.cpp
    src/layer.cpp
    src/formats.cpp)
target_link_libraries(_ffpsd PRIVATE ffpsd::ffpsd)

nanobind_add_stub(_ffpsd_stub
    MODULE _ffpsd
    OUTPUT _ffpsd.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_ffpsd>
    DEPENDS _ffpsd)

# Only this component goes into the wheel, not ffpsd's headers and CMake package.
install(TARGETS _ffpsd LIBRARY DESTINATION ffpsd COMPONENT python)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/_ffpsd.pyi" DESTINATION ffpsd COMPONENT python)

# nanobind is compiled into the module too, so its license goes with the others.
if(SKBUILD_METADATA_DIR)
    file(GLOB ffpsd_nanobind_license "${nanobind_DIR}/../../nanobind-*.dist-info/licenses/LICENSE")
    install(FILES ${ffpsd_nanobind_license} DESTINATION "${SKBUILD_METADATA_DIR}/licenses" RENAME nanobind.txt COMPONENT python)
endif()
