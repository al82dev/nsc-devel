# Verify saturation outside the decimal range and exactness traps inside it.
foreach(variable IN ITEMS NSC_EXECUTABLE FIXTURE_DIR OUTPUT_DIR RUNTIME_LIBRARY SOURCE_DIR)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing required parameter: ${variable}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(variable IN ITEMS TEMP TMP APPDATA LOCALAPPDATA)
    set(ENV{${variable}} "${OUTPUT_DIR}")
endforeach()

set(cases ExactControl)
foreach(sign IN ITEMS Positive Negative)
    foreach(position IN ITEMS Inner Outer)
        foreach(magnitude IN ITEMS 1 9 Tiny)
            foreach(operation IN ITEMS Add Sub)
                list(APPEND cases "${sign}${position}${magnitude}${operation}")
            endforeach()
        endforeach()
    endforeach()
endforeach()

if(DEFINED CASE_FILTER AND NOT CASE_FILTER IN_LIST cases)
    message(FATAL_ERROR "Unknown decimal saturation case: ${CASE_FILTER}")
endif()

set(failures "")
foreach(case IN LISTS cases)
    if(DEFINED CASE_FILTER AND NOT case STREQUAL CASE_FILTER)
        continue()
    endif()
    if(case MATCHES "Inner")
        set(expected 145)
    else()
        set(expected 0)
    endif()
    foreach(level IN ITEMS O0 O2)
        set(prefix "${OUTPUT_DIR}/${case}-${level}")
        set(program "${prefix}.exe")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env
                "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
                "NSC_LINKER=nslink.exe"
                "${NSC_EXECUTABLE}" --emit-project-oop-exe "${FIXTURE_DIR}/${case}.nsproj"
                -${level} -o "${program}" --no-artifact-cache --no-error-log
            WORKING_DIRECTORY "${SOURCE_DIR}"
            TIMEOUT 120
            RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
        file(WRITE "${prefix}.build.log"
            "compiler=${NSC_EXECUTABLE}\nruntime=${RUNTIME_LIBRARY}\nlinker=nslink.exe\n"
            "project=${FIXTURE_DIR}/${case}.nsproj\noptimization=-${level}\noutput=${program}\n"
            "flags=--no-artifact-cache --no-error-log\n${build_output}\n${build_error}\n"
            "result=${build_result}\n")
        if(NOT "${build_result}" STREQUAL "0")
            list(APPEND failures "${case} ${level}: build ${build_result}; see ${prefix}.build.log")
            continue()
        endif()
        execute_process(
            COMMAND "${program}" WORKING_DIRECTORY "${SOURCE_DIR}" TIMEOUT 10
            RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
        file(WRITE "${prefix}.run.log" "${run_output}\n${run_error}\nresult=${run_result}\n")
        if(NOT "${run_result}" STREQUAL "${expected}")
            list(APPEND failures
                "${case} ${level}: expected ${expected}, got ${run_result}; see ${prefix}.run.log")
            continue()
        endif()
        message(STATUS "${case} ${level}: expected ${expected}, got ${run_result}")
    endforeach()
endforeach()
if(failures)
    list(JOIN failures "\n" failure_report)
    message(FATAL_ERROR "Decimal saturating regressions failed:\n${failure_report}")
endif()
