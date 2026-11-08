file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(case IN ITEMS Safe Positive Negative)
    foreach(opt IN ITEMS 0 2)
        set(program "${OUTPUT_DIR}/${case}-O${opt}.exe")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${FIXTURE_DIR}/${case}.nsproj"
            -O${opt} --no-artifact-cache --no-error-log -o "${program}"
            RESULT_VARIABLE compile_result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT compile_result EQUAL 0)
            message(FATAL_ERROR "${case} O${opt} failed to compile: ${out}\n${err}")
        endif()
        execute_process(COMMAND "${program}" RESULT_VARIABLE run_result)
        if(case STREQUAL "Safe")
            set(expected 0)
        else()
            set(expected 147)
        endif()
        if(NOT run_result EQUAL expected)
            message(FATAL_ERROR "${case} O${opt}: expected ${expected}, got ${run_result}")
        endif()
    endforeach()
endforeach()
