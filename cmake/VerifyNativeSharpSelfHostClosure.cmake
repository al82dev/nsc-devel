if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Tanda 16 closure requires Python and roots")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_closure.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/closure.contract.json" --source-root "${SOURCE_ROOT}"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/tanda16-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Tanda 16 closure failed: ${out}${err}")
endif()
message(STATUS "${out}")
