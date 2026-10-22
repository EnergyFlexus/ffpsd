find_package(Threads REQUIRED)

# Static whatever the library is: it goes into both executables and is not installed.
add_library(img2ffpsd_common STATIC convert.cpp convert.hpp pictures.cpp pictures.hpp scan.cpp scan.hpp)
target_include_directories(img2ffpsd_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(img2ffpsd_common PUBLIC ffpsd::ffpsd Threads::Threads)
