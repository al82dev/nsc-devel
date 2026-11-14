# Fail when an in-process test executable is older than an input of its link step.
#
# Some suites (nsc_cc1_correctness_tests among them) do not run nsc.exe: they link the compiler
# libraries and run a Driver inside their own process. ctest does not build, so after a build of
# `--target nsc` the libraries are new while the test executable still holds the previous compiler.
# Such a run reports the old compiler's results as if they were the current ones, in both
# directions: defects already fixed appear as failures, and new defects can pass unseen.
#
# Inputs: TEST_BINARY (the executable) and TEST_INPUTS (files separated by '|': the libraries it
# links and its own sources). An input newer than the executable means the executable is stale.
if(NOT DEFINED TEST_BINARY OR NOT DEFINED TEST_INPUTS)
    message(FATAL_ERROR "VerifyTestBinaryCurrent.cmake needs TEST_BINARY and TEST_INPUTS")
endif()
if(NOT EXISTS "${TEST_BINARY}")
    message(FATAL_ERROR "test executable does not exist: ${TEST_BINARY}")
endif()
file(TIMESTAMP "${TEST_BINARY}" binary_time "%s" UTC)
string(REPLACE "|" ";" inputs "${TEST_INPUTS}")
set(stale "")
foreach(input IN LISTS inputs)
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "link input of ${TEST_BINARY} does not exist: ${input}")
    endif()
    file(TIMESTAMP "${input}" input_time "%s" UTC)
    if(input_time GREATER binary_time)
        list(APPEND stale "${input}")
    endif()
endforeach()
if(stale)
    list(JOIN stale "\n  " stale_text)
    get_filename_component(binary_name "${TEST_BINARY}" NAME_WE)
    message(FATAL_ERROR
        "${TEST_BINARY} is older than its inputs, so its results would describe a previous build:\n"
        "  ${stale_text}\n"
        "Build it before running these tests: cmake --build <build-dir> --target ${binary_name}")
endif()
message(STATUS "${TEST_BINARY} is current with ${TEST_INPUTS}")
