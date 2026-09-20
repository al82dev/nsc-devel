if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH26 requires nsc, Python and roots")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(compiler_project "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj")
set(positive_project "${SOURCE_ROOT}/selfhost/tests/module-set/positive/ModuleSetPositive.nsproj")
set(negative_project "${SOURCE_ROOT}/selfhost/tests/module-set/negative/ModuleSetNegative.nsproj")

foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${compiler_project}" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} compiler build failed: ${out}${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-module-set "${compiler_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/module-set-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} module set dump failed: ${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-module-exports "${compiler_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/module-exports-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} exports dump failed: ${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-module-set "${positive_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/positive-set-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} positive fixture failed: ${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-module-exports "${positive_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/positive-exports-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} positive exports failed: ${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-module-set "${negative_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/negative-set-${lower}.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH26 ${profile} negative fixture command failed: ${err}")
    endif()
endforeach()

execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_module_set.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/module-set.contract.json"
    --module-source "${SOURCE_ROOT}/selfhost/nsc/Sema/ModuleSet.ns"
    --analyzer-source "${SOURCE_ROOT}/selfhost/nsc/Sema/SemanticAnalyzer.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh26-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH26 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
