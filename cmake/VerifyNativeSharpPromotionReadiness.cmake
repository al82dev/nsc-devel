if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH14 requires nsc0, Python and roots")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_promotion_readiness.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/promotion-readiness.contract.json"
    --source-root "${SOURCE_ROOT}" --nsc0 "${NSC_EXECUTABLE}"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh14-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH14 readiness verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
