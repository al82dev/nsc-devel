if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "SH25 requires nsc, Python and roots")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(compiler_project "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj")

foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${compiler_project}" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH25 ${profile} compiler build failed: ${out}${err}")
    endif()
    execute_process(COMMAND "${candidate}" --dump-source-graph "${compiler_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/source-graph-${lower}.txt"
        RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH25 ${profile} source graph dump failed: ${err}")
    endif()
    execute_process(COMMAND "${candidate}" --check-project "${compiler_project}"
        OUTPUT_FILE "${OUTPUT_DIR}/check-project-${lower}.txt"
        RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH25 ${profile} project check failed: ${err}")
    endif()
endforeach()

execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_source_graph.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/source-graph.contract.json"
    --graph-source "${SOURCE_ROOT}/selfhost/nsc/Projects/SourceGraph.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh25-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH25 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
