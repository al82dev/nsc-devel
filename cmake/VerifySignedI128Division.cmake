# Compile and execute signed 128-bit division with the configured compiler.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(program "${OUTPUT_DIR}/SignedI128Division.exe")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
        "NSC_LINKER=${NSLINK_EXECUTABLE}"
        "${NSC_EXECUTABLE}" --emit-project-oop-exe "${PROJECT_FILE}"
        -O0 -o "${program}" --no-artifact-cache --no-error-log
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Regression build failed: ${build_result}\n${build_output}\n${build_error}")
endif()
execute_process(COMMAND "${program}" RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Regression execution failed: ${run_result}\n${run_output}\n${run_error}")
endif()


# Division by zero must still take the existing DivisionByZero trap (128 + 1).
set(zero_program "${OUTPUT_DIR}/SignedI128DivisionZero.exe")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
        "NSC_LINKER=${NSLINK_EXECUTABLE}"
        "${NSC_EXECUTABLE}" --emit-project-oop-exe "${ZERO_PROJECT_FILE}"
        -O0 -o "${zero_program}" --no-artifact-cache --no-error-log
    RESULT_VARIABLE zero_build OUTPUT_VARIABLE zero_output ERROR_VARIABLE zero_error)
if(NOT zero_build EQUAL 0)
    message(FATAL_ERROR "Zero-divisor build failed: ${zero_build}\n${zero_output}\n${zero_error}")
endif()
execute_process(COMMAND "${zero_program}" RESULT_VARIABLE zero_result)
if(NOT zero_result EQUAL 129)
    message(FATAL_ERROR "Expected division-by-zero trap 129, got ${zero_result}")
endif()
