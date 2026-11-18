# Compile and execute a fixed matrix multiplication with forwarded const dimensions.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(opt IN ITEMS 0 2)
set(program "${OUTPUT_DIR}/FixedMatrixConstForward-O${opt}.exe")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
        "NSC_LINKER=${NSLINK_EXECUTABLE}"
        "${NSC_EXECUTABLE}" --emit-project-oop-exe "${PROJECT_FILE}"
        -O${opt} -o "${program}" --no-artifact-cache --no-error-log
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Regression build failed: ${build_result}\n${build_output}\n${build_error}")
endif()
execute_process(COMMAND "${program}" RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Regression execution failed: ${run_result}\n${run_output}\n${run_error}")
endif()
endforeach()


# Concrete invalid arguments must not be hidden by a different open type parameter.
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --check-project-generics "${INVALID_PROJECT_FILE}" --no-error-log
    RESULT_VARIABLE invalid_result OUTPUT_VARIABLE invalid_output ERROR_VARIABLE invalid_error)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "Invalid concrete const argument unexpectedly compiled")
endif()
string(CONCAT invalid_diagnostic "${invalid_output}" "${invalid_error}")
if(NOT invalid_diagnostic MATCHES "requires a compile-time value argument")
    message(FATAL_ERROR "Unexpected invalid-argument diagnostic: ${invalid_diagnostic}")
endif()


# A type parameter is not a const value, even when another parameter is open.
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --check-project-generics "${INVALID_OPEN_PROJECT_FILE}" --no-error-log
    RESULT_VARIABLE open_result OUTPUT_VARIABLE open_output ERROR_VARIABLE open_error)
if(open_result EQUAL 0)
    message(FATAL_ERROR "Type parameter used as a const argument unexpectedly compiled")
endif()
string(CONCAT open_diagnostic "${open_output}" "${open_error}")
if(NOT open_diagnostic MATCHES "requires a compile-time value argument")
    message(FATAL_ERROR "Unexpected open-type diagnostic: ${open_diagnostic}")
endif()
