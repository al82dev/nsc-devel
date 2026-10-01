cmake_minimum_required(VERSION 3.20)

foreach(required NSC_EXECUTABLE RUNTIME_LIBRARY FIXTURE_ROOT OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "T43 requires -D${required}=...")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")

set(positive "${OUTPUT_DIR}/T43Callables.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${FIXTURE_ROOT}/T43Callables.nsproj" -O0 -o "${positive}" --no-error-log
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "T43 positive fixture did not compile (exit ${compile_result})\n${compile_out}\n${compile_err}")
endif()
execute_process(COMMAND "${positive}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "T43 positive fixture returned ${run_result}, expected 0")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --dump-project-oop-llvm
        "${FIXTURE_ROOT}/T43Callables.nsproj" --no-error-log
    RESULT_VARIABLE llvm_result OUTPUT_VARIABLE llvm_out ERROR_VARIABLE llvm_err)
if(NOT llvm_result EQUAL 0)
    message(FATAL_ERROR "T43 LLVM dump failed (exit ${llvm_result})\n${llvm_out}\n${llvm_err}")
endif()
string(FIND "${llvm_out}" "inlinehint" inline_hint)
if(inline_hint EQUAL -1)
    message(FATAL_ERROR "T43 free inline function did not preserve its LLVM inline hint\n${llvm_out}")
endif()

function(expect_t43_failure case_name expected_text)
    set(project "${FIXTURE_ROOT}/negative/${case_name}/${case_name}.nsproj")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${project}" -O0
            -o "${OUTPUT_DIR}/${case_name}.exe" --no-error-log
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(log "${out}\n${err}")
    if(result EQUAL 0)
        message(FATAL_ERROR "T43 negative ${case_name} compiled successfully")
    endif()
    string(FIND "${log}" "${expected_text}" expected_at)
    if(expected_at EQUAL -1)
        message(FATAL_ERROR "T43 negative ${case_name} missed '${expected_text}'\n${log}")
    endif()
endfunction()

expect_t43_failure(UndefinedCall "is called but has no definition")
expect_t43_failure(AmbiguousArgument "more than one compatible delegate target")
expect_t43_failure(IncompatibleFreeDelegate "no method in the method group exactly matches")

message(STATUS "T43 cross-unit functions, contextual delegates, FIFO events, inline and catch(Type) passed")
