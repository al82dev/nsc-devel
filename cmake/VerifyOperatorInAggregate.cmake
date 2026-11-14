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
set(throw_project "${integration_dir}/../operator_compound_temporary_throw/Case.nsproj")
set(replace_throw_project "${integration_dir}/../operator_value_replace_throw/Case.nsproj")
foreach(level IN ITEMS O0 O2)
    set(program "${OUTPUT_DIR}/OperatorCompoundThrow_${level}.exe")
    execute_process(
        COMMAND cmake -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${throw_project}"
            "-${level}" -o "${program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "${level} throwing operator build failed: ${build_result}\n${build_output}\n${build_error}")
    endif()
    execute_process(COMMAND "${program}" TIMEOUT 10 RESULT_VARIABLE run_result)
    if(NOT "${run_result}" STREQUAL "42")
        message(FATAL_ERROR "${level} throwing operator cleanup failed: ${run_result}")
    endif()
    set(replace_program "${OUTPUT_DIR}/OperatorValueReplaceThrow_${level}.exe")
    execute_process(
        COMMAND cmake -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${replace_throw_project}"
            "-${level}" -o "${replace_program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "${level} throwing value replacement build failed: ${build_result}\n${build_output}\n${build_error}")
    endif()
    execute_process(COMMAND "${replace_program}" TIMEOUT 10 RESULT_VARIABLE run_result)
    if(NOT "${run_result}" STREQUAL "42")
        message(FATAL_ERROR "${level} throwing value replacement cleanup failed: ${run_result}")
    endif()
endforeach()
set(invalid_project "${integration_dir}/../operator_in_aggregate_invalid/Invalid.nsproj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --check-project-method-bodies "${invalid_project}"
    RESULT_VARIABLE invalid_result OUTPUT_VARIABLE invalid_output ERROR_VARIABLE invalid_error)
if(invalid_result EQUAL 0 OR NOT "${invalid_output}${invalid_error}" MATCHES "compound assignment requires compatible numeric operands")
    message(FATAL_ERROR "missing operator was not rejected as expected: ${invalid_result}\n${invalid_output}\n${invalid_error}")
endif()
if(NOT "${invalid_output}${invalid_error}" MATCHES "no viable Stage 38 overload")
    message(FATAL_ERROR "temporary ref operand was not rejected as expected: ${invalid_result}\n${invalid_output}\n${invalid_error}")
endif()
if(NOT "${invalid_output}${invalid_error}" MATCHES "compound operator result must have the destination type or an implicit user conversion")
    message(FATAL_ERROR "missing result conversion was not rejected as expected: ${invalid_result}\n${invalid_output}\n${invalid_error}")
endif()
