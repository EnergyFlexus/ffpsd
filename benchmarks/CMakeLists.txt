# Numbers from a Release build only mean anything; the bench-release preset makes one.
add_executable(ffpsd_benchmarks
    document_benchmark.cpp
    formats_benchmark.cpp
    layer_benchmark.cpp
    support/benchmark_support.hpp)

# A build of another version takes the files from this checkout: Git LFS fills them only here.
set(FFPSD_BENCHMARK_DATA_DIR "${PROJECT_SOURCE_DIR}/tests/data" CACHE PATH "The tests/data the benchmarks read")

target_include_directories(ffpsd_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ffpsd_benchmarks PRIVATE FFPSD_BENCHMARK_DATA_DIR="${FFPSD_BENCHMARK_DATA_DIR}")
target_link_libraries(ffpsd_benchmarks PRIVATE ffpsd::ffpsd benchmark::benchmark_main)
