if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH15 requires nsc0, Python and roots")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(nsc1 "${OUTPUT_DIR}/nsc1.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" -O2 -g0 -o "${nsc1}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH15 nsc1 build failed: ${out}${err}")
endif()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_image_replay_retired.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/image-replay-retirement.contract.json"
        --source-root "${SOURCE_ROOT}" --nsc1 "${nsc1}" --output-dir "${OUTPUT_DIR}"
        --report "${OUTPUT_DIR}/sh15-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH15 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
