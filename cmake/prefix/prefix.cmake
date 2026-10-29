# The compiler option that puts a header in front of every file it compiles.
function(ffpsd_prefix_option variable header)
    if(MSVC)
        set(${variable} "/FI${header}" PARENT_SCOPE)
    else()
        # As two list items: a source file's COMPILE_OPTIONS take no SHELL: form.
        set(${variable} -include "${header}" PARENT_SCOPE)
    endif()
endfunction()

# The command that runs symbols.cmake in a mode, with the definitions given after it.
function(ffpsd_symbols_command variable mode)
    if(MSVC)
        set(tool "-DDUMPBIN=${CMAKE_LINKER}")
    else()
        set(tool "-DNM=${CMAKE_NM}" "-DLEADING_UNDERSCORE=${APPLE}")
    endif()
    set(${variable} "${CMAKE_COMMAND}" "-DMODE=${mode}" ${tool} ${ARGN} -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/symbols.cmake"
        PARENT_SCOPE)
endfunction()

# As CMAKE_PROJECT_INCLUDE of a library built on its own, it forces the header into all of that library;
# headers in FFPSD_PREFIX_INCLUDE are found before the library's own.
if(FFPSD_PREFIX_HEADER)
    ffpsd_prefix_option(ffpsd_option "${FFPSD_PREFIX_HEADER}")
    add_compile_options("${ffpsd_option}")
endif()
if(FFPSD_PREFIX_INCLUDE)
    include_directories(BEFORE "${FFPSD_PREFIX_INCLUDE}")
endif()
