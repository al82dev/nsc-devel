# Verify checked/default scale-zero decimal overflow and representable control.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(variable IN ITEMS TEMP TMP APPDATA LOCALAPPDATA)
    set(ENV{${variable}} "${OUTPUT_DIR}")
endforeach()
foreach(case IN ITEMS CheckedAdd DefaultAdd CheckedSub DefaultSub Valid)
    if(case STREQUAL "Valid")
        set(expected 0)
    else()
        set(expected 145)
    endif()
    foreach(level IN ITEMS O0 O2)
        set(program "${OUTPUT_DIR}/${case}-${level}.exe")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env
                "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
                "NSC_LINKER=nslink.exe"
                "${NSC_EXECUTABLE}" --emit-project-oop-exe "${FIXTURE_DIR}/${case}.nsproj"
                -${level} -o "${program}" --no-artifact-cache --no-error-log
            WORKING_DIRECTORY "${SOURCE_DIR}"
            RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
        if(NOT build_result EQUAL 0)
            message(FATAL_ERROR "${case} ${level} build failed: ${build_result}\n${build_output}\n${build_error}")
        endif()
        execute_process(COMMAND "${program}" WORKING_DIRECTORY "${SOURCE_DIR}"
                        RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
        if(NOT run_result EQUAL expected)
            message(FATAL_ERROR "${case} ${level} expected ${expected}, got ${run_result}\n${run_output}\n${run_error}")
        endif()
    endforeach()
endforeach()
