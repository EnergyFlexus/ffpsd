# A window app: no console on Windows.
add_executable(img2ffpsd_ui WIN32 main.cpp)
target_link_libraries(img2ffpsd_ui PRIVATE ffpsd::ffpsd imgui)

# CoreText finds the system's fonts on macOS.
if(APPLE)
    target_link_libraries(img2ffpsd_ui PRIVATE "-framework CoreText" "-framework CoreFoundation")
endif()

# Beside img2ffpsd, and like it finds a shared libffpsd through a path relative to itself.
file(RELATIVE_PATH ffpsd_bin_to_lib "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
if(APPLE)
    set_target_properties(img2ffpsd_ui PROPERTIES INSTALL_RPATH "@loader_path/${ffpsd_bin_to_lib}")
elseif(UNIX)
    set_target_properties(img2ffpsd_ui PROPERTIES INSTALL_RPATH "$ORIGIN/${ffpsd_bin_to_lib}")
endif()

install(TARGETS img2ffpsd_ui RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
