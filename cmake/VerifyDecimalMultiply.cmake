# Verify decimal multiplication in independent processes at O0 and O2.
# Inputs: NSC_EXECUTABLE, NSLINK_EXECUTABLE, RUNTIME_LIBRARY, FIXTURE_DIR,
# OUTPUT_DIR and SOURCE_DIR. The caller is responsible for tool provenance.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(variable IN ITEMS TEMP TMP APPDATA LOCALAPPDATA)
    set(ENV{${variable}} "${OUTPUT_DIR}")
endforeach()
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")
set(ENV{NSC_LINKER} "${NSLINK_EXECUTABLE}")
set(summary "${OUTPUT_DIR}/observations.txt")
file(WRITE "${summary}" "Decimal multiplication observations\n")
set(failures 0)
foreach(case IN ITEMS MulSign MulWide MulHighProduct MulScaleOverflow MulCoefficientOverflow
                      MulSaturatingPositive MulSaturatingNegative)
    if(case STREQUAL "MulScaleOverflow" OR case STREQUAL "MulCoefficientOverflow" OR
       case STREQUAL "MulHighProduct")
        set(expected 145)
    else()
        set(expected 0)
    endif()
    foreach(level IN ITEMS O0 O2)
        set(prefix "${OUTPUT_DIR}/${case}-${level}")
        set(program "${prefix}.exe")
        execute_process(
            COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${FIXTURE_DIR}/${case}.nsproj"
                -${level} -o "${program}" --no-artifact-cache --no-error-log
            WORKING_DIRECTORY "${SOURCE_DIR}"
            TIMEOUT 120
            RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
        file(WRITE "${prefix}.build.log" "exit=${build_result}\n${build_output}\n${build_error}\n")
        if(NOT "${build_result}" STREQUAL "0")
            file(APPEND "${summary}" "${case} ${level}: build=${build_result}; run=NOT_RUN\n")
            math(EXPR failures "${failures} + 1")
            continue()
        endif()
        execute_process(COMMAND "${program}" WORKING_DIRECTORY "${SOURCE_DIR}" TIMEOUT 10
            RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
        file(WRITE "${prefix}.run.log" "exit=${run_result}\n${run_output}\n${run_error}\n")
        file(APPEND "${summary}" "${case} ${level}: build=0; run=${run_result}; expected=${expected}\n")
        if(NOT "${run_result}" STREQUAL "${expected}")
            math(EXPR failures "${failures} + 1")
        endif()
    endforeach()
endforeach()
file(APPEND "${summary}" "Contract mismatches or build failures: ${failures}\n")
if(NOT failures EQUAL 0)
    message(FATAL_ERROR "Decimal multiplication has ${failures} failures; see ${summary}")
endif()
message(STATUS "Decimal multiplication observations collected: ${summary}")
