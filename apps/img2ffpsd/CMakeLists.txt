find_package(Threads REQUIRED)

add_executable(img2ffpsd main.cpp)
target_link_libraries(img2ffpsd PRIVATE ffpsd::ffpsd Threads::Threads)

# Installed, it finds a shared libffpsd through a path relative to itself, so the
# package works wherever it is copied. Windows needs none: the DLL sits beside it.
file(RELATIVE_PATH ffpsd_bin_to_lib "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
if(APPLE)
    set_target_properties(img2ffpsd PROPERTIES INSTALL_RPATH "@loader_path/${ffpsd_bin_to_lib}")
elseif(UNIX)
    set_target_properties(img2ffpsd PROPERTIES INSTALL_RPATH "$ORIGIN/${ffpsd_bin_to_lib}")
endif()

install(TARGETS img2ffpsd RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
