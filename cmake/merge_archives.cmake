# Adds the objects of the archives listed in the file ARCHIVES to the static LIBRARY,
# with the archiver TOOL of STYLE msvc, apple or gnu.
file(STRINGS "${ARCHIVES}" archives)
set(merged "${LIBRARY}.merged")

if(STYLE STREQUAL "msvc")
    execute_process(COMMAND "${TOOL}" /lib /nologo "/out:${merged}" "${LIBRARY}" ${archives} COMMAND_ERROR_IS_FATAL ANY)
elseif(STYLE STREQUAL "apple")
    execute_process(COMMAND "${TOOL}" -static -o "${merged}" "${LIBRARY}" ${archives} COMMAND_ERROR_IS_FATAL ANY)
else()
    # ar takes in whole archives only through a script on its standard input.
    set(script "CREATE ${merged}\n")
    foreach(archive IN ITEMS "${LIBRARY}" ${archives})
        string(APPEND script "ADDLIB ${archive}\n")
    endforeach()
    string(APPEND script "SAVE\nEND\n")
    file(WRITE "${merged}.mri" "${script}")
    execute_process(COMMAND "${TOOL}" -M INPUT_FILE "${merged}.mri" COMMAND_ERROR_IS_FATAL ANY)
    file(REMOVE "${merged}.mri")
endif()

file(RENAME "${merged}" "${LIBRARY}")
