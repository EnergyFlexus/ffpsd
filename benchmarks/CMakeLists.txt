# Numbers from a Release build only mean anything; the bench-release preset makes one.
add_executable(ffpsd_benchmarks
    psd_benchmark.cpp)

target_compile_definitions(ffpsd_benchmarks PRIVATE FFPSD_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
target_link_libraries(ffpsd_benchmarks PRIVATE ffpsd::ffpsd benchmark::benchmark_main)
