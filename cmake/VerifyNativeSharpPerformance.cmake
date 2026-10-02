if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED CTEST_EXECUTABLE OR NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "SH50 requires Python, roots, SH41 products, CTest and the current build")
endif()
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-nslink.exe")
foreach(required IN ITEMS PYTHON_EXECUTABLE candidate CTEST_EXECUTABLE)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH50 missing ${required}: ${${required}}")
    endif()
endforeach()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_performance.py"
    --source-root "${SOURCE_ROOT}" --contract "${SOURCE_ROOT}/selfhost/contracts/performance.contract.json"
    --compiler "${candidate}" --ctest "${CTEST_EXECUTABLE}" --build-dir "${BUILD_DIR}"
    --output "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh50-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH50 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
