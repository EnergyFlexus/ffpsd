# One folder per app; each says here what it needs.

# img2ffpsd reads PNG and JPEG files.
if(FFPSD_WITH_PNG AND FFPSD_WITH_JPEG)
    add_subdirectory(img2ffpsd)
else()
    message(STATUS "img2ffpsd is skipped: it needs FFPSD_WITH_PNG and FFPSD_WITH_JPEG")
endif()
