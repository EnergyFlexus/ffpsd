# A window app: no console on Windows.
add_executable(img2ffpsd_ui WIN32 main.cpp)
set_target_properties(img2ffpsd_ui PROPERTIES OUTPUT_NAME img2ffpsd-ui)
target_link_libraries(img2ffpsd_ui PRIVATE img2ffpsd_common imgui)

# CoreText finds the system's fonts on macOS.
if(APPLE)
    target_link_libraries(img2ffpsd_ui PRIVATE "-framework CoreText" "-framework CoreFoundation")
endif()

img2ffpsd_install(img2ffpsd_ui)
