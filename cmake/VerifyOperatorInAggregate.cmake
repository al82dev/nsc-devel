# Compile and run the aggregate in-operator regression without artifact cache.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(level IN ITEMS O0 O2)
    set(program "${OUTPUT_DIR}/OperatorInAggregate_${level}.exe")
    execute_process(
        COMMAND cmake -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${PROJECT_FILE}"
            "-${level}" -o "${program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "${level} build failed: ${build_result}\n${build_output}\n${build_error}")
    endif()
    execute_process(COMMAND "${program}" TIMEOUT 10
        RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
    if(NOT "${run_result}" STREQUAL "0")
        message(FATAL_ERROR "${level} run failed: ${run_result}\n${run_output}\n${run_error}")
    endif()
endforeach()

get_filename_component(integration_dir "${PROJECT_FILE}" DIRECTORY)
set(invalid_project "${integration_dir}/../operator_in_aggregate_invalid/Invalid.nsproj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --check-project-method-bodies "${invalid_project}"
    RESULT_VARIABLE invalid_result OUTPUT_VARIABLE invalid_output ERROR_VARIABLE invalid_error)
if(invalid_result EQUAL 0 OR NOT "${invalid_output}${invalid_error}" MATCHES "compound assignment requires compatible numeric operands")
    message(FATAL_ERROR "missing operator was not rejected as expected: ${invalid_result}\n${invalid_output}\n${invalid_error}")
endif()
if(NOT "${invalid_output}${invalid_error}" MATCHES "compound user operator requires an unconverted destination and value or in parameters")
    message(FATAL_ERROR "unsupported ref operator was not rejected as expected: ${invalid_result}\n${invalid_output}\n${invalid_error}")
endif()
