# Observe independent decimal multiplication/division processes without hiding failures.
# Inputs: NSC_EXECUTABLE, NSLINK_EXECUTABLE, RUNTIME_LIBRARY, FIXTURE_DIR,
# OUTPUT_DIR and SOURCE_DIR. The caller must establish the provenance of each input.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(variable IN ITEMS TEMP TMP APPDATA LOCALAPPDATA)
    set(ENV{${variable}} "${OUTPUT_DIR}")
endforeach()
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")
set(ENV{NSC_LINKER} "${NSLINK_EXECUTABLE}")
set(summary "${OUTPUT_DIR}/observations.txt")
file(WRITE "${summary}" "Decimal multiplication and division observations\n")
set(failures 0)
set(selected 0)
foreach(case IN ITEMS MulSign DivLeftSign DivRightSign MulWide DivWideDividend DivWideDivisor
                      DivHalfUp MulScaleOverflow MulCoefficientOverflow MulSaturatingPositive
                      MulSaturatingNegative MulHighProduct DivZero DivZeroScaled DivScaleReduction
                      DivOverflowChecked DivOverflowSaturating DivNegativeHalfUp DivMaxOverTwo
                      DivInteriorInexactChecked DivInteriorInexactSat DivWideNonzeroDivisor DivZeroResult
                      DivScaleOverflow DivInteriorNegativeSat DivOverflowNegativeChecked DivRoundingCases
                      DivMultiScaleReduction DivExponent56Reduction)
    if(DEFINED CASE_PREFIX AND NOT case MATCHES "^${CASE_PREFIX}")
        continue()
    endif()
    math(EXPR selected "${selected} + 1")
    if(case STREQUAL "MulScaleOverflow" OR case STREQUAL "MulCoefficientOverflow" OR case STREQUAL "MulHighProduct"
       OR case STREQUAL "DivOverflowChecked" OR case STREQUAL "DivInteriorInexactChecked"
       OR case STREQUAL "DivInteriorInexactSat" OR case STREQUAL "DivScaleOverflow"
       OR case STREQUAL "DivInteriorNegativeSat" OR case STREQUAL "DivOverflowNegativeChecked")
        set(expected 145)
    elseif(case STREQUAL "DivZero" OR case STREQUAL "DivZeroScaled")
        # Adopt the same DivisionByZero trap as integer division; this is a new decimal decision.
        set(expected 129)
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
if(selected EQUAL 0)
    message(FATAL_ERROR "No decimal cases match CASE_PREFIX=${CASE_PREFIX}")
endif()
file(APPEND "${summary}" "Contract mismatches or build failures: ${failures}\n")
if(NOT failures EQUAL 0)
    message(FATAL_ERROR "Decimal observations contain ${failures} failures; see ${summary}")
endif()
message(STATUS "Decimal multiplication and division checks passed: ${summary}")
