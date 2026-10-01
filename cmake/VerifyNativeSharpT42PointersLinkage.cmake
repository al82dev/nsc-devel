cmake_minimum_required(VERSION 3.20)

foreach(required NSC_EXECUTABLE RUNTIME_LIBRARY FIXTURE_ROOT OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "T42 requires -D${required}=...")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")

set(positive "${OUTPUT_DIR}/T42Pointers.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${FIXTURE_ROOT}/T42Pointers.nsproj" -O0 -o "${positive}" --no-error-log
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "T42 positive fixture did not compile (exit ${compile_result})\n${compile_out}\n${compile_err}")
endif()
execute_process(COMMAND "${positive}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "T42 positive fixture returned ${run_result}, expected 0")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --dump-project-oop-llvm
        "${FIXTURE_ROOT}/T42Pointers.nsproj" --no-error-log
    RESULT_VARIABLE llvm_result OUTPUT_VARIABLE llvm_out ERROR_VARIABLE llvm_err)
if(NOT llvm_result EQUAL 0)
    message(FATAL_ERROR "T42 LLVM dump failed (exit ${llvm_result})\n${llvm_out}\n${llvm_err}")
endif()
string(FIND "${llvm_out}" "load volatile" volatile_load)
string(FIND "${llvm_out}" "store volatile" volatile_store)
if(volatile_load EQUAL -1 OR volatile_store EQUAL -1)
    message(FATAL_ERROR "T42 volatile local did not produce both volatile load and store\n${llvm_out}")
endif()

function(expect_t42_failure case_name expected_text)
    set(project "${FIXTURE_ROOT}/negative/${case_name}/${case_name}.nsproj")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib "${project}" -O0
            -o "${OUTPUT_DIR}/${case_name}.lib" --no-error-log
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(log "${out}\n${err}")
    if(result EQUAL 0)
        message(FATAL_ERROR "T42 negative ${case_name} compiled successfully")
    endif()
    string(FIND "${log}" "${expected_text}" expected_at)
    if(expected_at EQUAL -1)
        message(FATAL_ERROR "T42 negative ${case_name} missed '${expected_text}'\n${log}")
    endif()
endfunction()

expect_t42_failure(DiscardInit "would discard pointee constness")
expect_t42_failure(WritePointee "left side of assignment is not assignable")
expect_t42_failure(ReassignBinding "left side of assignment is not assignable")
expect_t42_failure(OnePastDeref "one-past-end pointer")
expect_t42_failure(BadStaticCast "static_cast cannot convert")
expect_t42_failure(BadConstCast "const_cast cannot convert")
expect_t42_failure(BadReinterpretConst "reinterpret_cast cannot convert")
expect_t42_failure(CStyleDiscard "C-style cast cannot discard")
expect_t42_failure(CallDiscard "no viable overload")
expect_t42_failure(ReturnDiscard "return would discard")
expect_t42_failure(NamespaceStatic "file-local linkage are not supported")
expect_t42_failure(ExternState "extern namespace variables are not supported")

message(STATUS "T42 pointer constness, one-past, named casts, volatile and linkage checks passed")
