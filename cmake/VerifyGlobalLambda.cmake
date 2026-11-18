# Compile and execute a noncapturing lambda owned by a global function.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")
set(ENV{NSC_LINKER} "${NSLINK_EXECUTABLE}")
foreach(level IN ITEMS O0 O2)
    set(program "${OUTPUT_DIR}/GlobalLambda-${level}.exe")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${PROJECT_FILE}"
            -${level} -o "${program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "Regression build ${level} failed: ${build_result}\n${build_output}\n${build_error}")
    endif()
    execute_process(COMMAND "${program}" RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
    if(NOT run_result EQUAL 0)
        message(FATAL_ERROR "Regression execution ${level} failed: ${run_result}\n${run_output}\n${run_error}")
    endif()
endforeach()
