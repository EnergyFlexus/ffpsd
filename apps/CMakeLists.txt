# One folder per app; each says here what it needs.

# img2ffpsd and its UI read PNG and JPEG files.
if(FFPSD_WITH_PNG AND FFPSD_WITH_JPEG)
    add_subdirectory(img2ffpsd)
    add_subdirectory(img2ffpsd_ui)
else()
    message(STATUS "img2ffpsd and img2ffpsd_ui are skipped: they need FFPSD_WITH_PNG and FFPSD_WITH_JPEG")
endif()
