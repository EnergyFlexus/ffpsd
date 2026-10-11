# Installed beside each other, they find a shared libffpsd through a path relative to themselves, so the
# package works wherever it is copied. Windows needs none: the DLL sits beside them.
function(img2ffpsd_install target)
    file(RELATIVE_PATH bin_to_lib "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
    if(APPLE)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/${bin_to_lib}")
    elseif(UNIX)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/${bin_to_lib}")
    endif()
    install(TARGETS ${target} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endfunction()

# What the command line and the window share: finding the pictures and converting them.
add_subdirectory(common)
add_subdirectory(cli)

# The window draws with Dear ImGui.
if(FFPSD_WITH_IMGUI)
    add_subdirectory(ui)
else()
    message(STATUS "img2ffpsd-ui is skipped: it needs FFPSD_WITH_IMGUI")
endif()
