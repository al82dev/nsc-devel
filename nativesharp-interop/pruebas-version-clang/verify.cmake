if(NOT DEFINED NSC_EXPECTED_CLANG_VERSION OR NOT DEFINED NSC_TEST_BINARY_DIR)
    message(FATAL_ERROR "Expected version and test binary directory are required")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}" -B "${NSC_TEST_BINARY_DIR}"
        "-DNSC_EXPECTED_CLANG_VERSION=${NSC_EXPECTED_CLANG_VERSION}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0)
    message(FATAL_ERROR "Incompatible Clang package was accepted")
endif()
string(CONCAT text "${output}" "${error}")
if(NOT text MATCHES "22\\.1\\.0" OR NOT text MATCHES "Clang")
    message(FATAL_ERROR "Failure did not identify the incompatible Clang package: ${text}")
endif()
message(STATUS "Incompatible Clang 22.1.0 rejected by exact package lookup")
