if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH28 requires nsc0, Python and roots")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(project "${SOURCE_ROOT}/selfhost/tests/backend-general/BackendGeneral.nsproj")
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH28 ${profile} compiler failed: ${out}${err}")
    endif()
    foreach(stage IN ITEMS hir mir cfg dominance ssa)
        execute_process(COMMAND "${candidate}" --dump-project-ir "${project}" "${stage}"
            OUTPUT_FILE "${OUTPUT_DIR}/${stage}-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "SH28 ${profile}/${stage} failed: ${err}")
        endif()
    endforeach()
endforeach()
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_program_ir.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/program-ir.contract.json"
    --ir-source "${SOURCE_ROOT}/selfhost/nsc/IR/IntermediateRepresentation.ns"
    --backend-source "${SOURCE_ROOT}/selfhost/nsc/CodeGen/LlvmBackend.ns"
    --driver-source "${SOURCE_ROOT}/selfhost/nsc/Driver/Program.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh28-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH28 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
