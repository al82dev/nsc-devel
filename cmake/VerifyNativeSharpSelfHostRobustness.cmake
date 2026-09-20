if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH13 requires nsc0, Python, roots and linker")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(fixed "${OUTPUT_DIR}/fixed-point")
execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DNSC_EXECUTABLE=${NSC_EXECUTABLE}" "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}"
    "-DSOURCE_ROOT=${SOURCE_ROOT}" "-DOUTPUT_DIR=${fixed}" "-DLINKER_EXECUTABLE=${LINKER_EXECUTABLE}"
    -P "${SOURCE_ROOT}/cmake/VerifyNativeSharpFixedPoint.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH13 fixed-point preparation failed: ${out}${err}")
endif()
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_robustness.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/robustness.contract.json"
    --source-root "${SOURCE_ROOT}" --nsc0 "${NSC_EXECUTABLE}"
    --nsc2 "${fixed}/nsc2.exe" --nsc3 "${fixed}/nsc3-1.exe"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh13-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH13 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
