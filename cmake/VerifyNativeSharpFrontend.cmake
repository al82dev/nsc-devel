if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH05 requires nsc, Python, source root and output dir")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(project "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj")
set(next_o0 "${OUTPUT_DIR}/nsc-next-o0.exe")
set(next_o2 "${OUTPUT_DIR}/nsc-next-o2.exe")

foreach(optimization IN ITEMS O0 O2)
    string(TOLOWER "${optimization}" optimization_lower)
    set(output "${OUTPUT_DIR}/nsc-next-${optimization_lower}.exe")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${project}"
            "-${optimization}" -g0 -o "${output}"
        RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err)
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "SH05 ${optimization} compilation failed:\n${compile_out}${compile_err}")
    endif()
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --verify-pe "${output}"
        RESULT_VARIABLE verify_result OUTPUT_VARIABLE verify_out ERROR_VARIABLE verify_err)
    if(NOT verify_result EQUAL 0)
        message(FATAL_ERROR "SH05 ${optimization} PE verification failed:\n${verify_out}${verify_err}")
    endif()
endforeach()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_frontend.py"
        --source-root "${SOURCE_ROOT}"
        --nsc0 "${NSC_EXECUTABLE}"
        --nsc-next-o0 "${next_o0}"
        --nsc-next-o2 "${next_o2}"
        --contract "${SOURCE_ROOT}/selfhost/contracts/frontend.contract.json"
        --output-dir "${OUTPUT_DIR}/differential"
        --report "${OUTPUT_DIR}/sh05-report.json"
    RESULT_VARIABLE gate_result OUTPUT_VARIABLE gate_out ERROR_VARIABLE gate_err)
if(NOT gate_result EQUAL 0)
    message(FATAL_ERROR "SH05 differential gate failed:\n${gate_out}${gate_err}")
endif()
