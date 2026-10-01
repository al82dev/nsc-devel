cmake_minimum_required(VERSION 3.20)

foreach(required NSC_EXECUTABLE RUNTIME_LIBRARY FIXTURE_ROOT OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "T40 requires -D${required}=...")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")

set(positive "${OUTPUT_DIR}/T40Flow.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${FIXTURE_ROOT}/T40Flow.nsproj" -O0 -o "${positive}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR
        "T40 positive fixture did not compile (exit ${compile_result})\n${compile_out}\n${compile_err}")
endif()

execute_process(COMMAND "${positive}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "T40 positive fixture returned ${run_result}, expected 0")
endif()

function(expect_t40_failure case_name expected_text forbidden_text)
    set(project "${FIXTURE_ROOT}/negative/${case_name}/${case_name}.nsproj")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib "${project}" -O0
            -o "${OUTPUT_DIR}/${case_name}.lib"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(log "${out}\n${err}")
    if(result EQUAL 0)
        message(FATAL_ERROR "T40 negative ${case_name} compiled successfully")
    endif()
    string(FIND "${log}" "${expected_text}" expected_at)
    if(expected_at EQUAL -1)
        message(FATAL_ERROR
            "T40 negative ${case_name} missed '${expected_text}'\n${log}")
    endif()
    if(NOT "${forbidden_text}" STREQUAL "")
        string(FIND "${log}" "${forbidden_text}" forbidden_at)
        if(NOT forbidden_at EQUAL -1)
            message(FATAL_ERROR
                "T40 negative ${case_name} regressed to '${forbidden_text}'\n${log}")
        endif()
    endif()
endfunction()

expect_t40_failure(SkippedOut "use of unassigned local variable 'value'" "")
expect_t40_failure(SizeofDoesNotAssign "use of unassigned local variable 'value'" "")
expect_t40_failure(
    RawPointerWrite
    "raw pointer does not establish definite assignment"
    "Store(&value): error: use of unassigned")

message(STATUS "T40 condition-flow and unevaluated-context checks passed")
