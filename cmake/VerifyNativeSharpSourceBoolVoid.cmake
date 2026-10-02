if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH20 requires nsc0, Python, roots and linkers")
endif()
set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(llvm_readobj "${llvm_bin}/llvm-readobj.exe")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(adapter "${OUTPUT_DIR}/adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj"
    -O0 -g0 -o "${adapter}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH20 adapter failed: ${out}${err}")
endif()
set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH20 implib failed: ${out}${err}")
endif()
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH20 ${profile} compiler failed: ${out}${err}")
    endif()
    set(object "${OUTPUT_DIR}/scalars-${lower}.obj")
    execute_process(COMMAND "${candidate}" --emit-object
        "${SOURCE_ROOT}/selfhost/tests/codegen/source-bool-void.ns" "-${profile}" -o "${object}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0 OR NOT EXISTS "${object}")
        message(FATAL_ERROR "SH20 ${profile} object failed: ${out}${err}")
    endif()
    execute_process(COMMAND "${llvm_readobj}" --symbols --relocations "${object}"
        OUTPUT_FILE "${OUTPUT_DIR}/scalars-${lower}.readobj.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH20 ${profile} inspection failed: ${err}")
    endif()
    foreach(route IN ITEMS nslink)
        set(tool "${nslink}")
        execute_process(COMMAND "${adapter}" "${route}" "${tool}" "${object}" "${import_library}"
            "${OUTPUT_DIR}/${route}-${lower}.rsp" "${OUTPUT_DIR}/${route}-${lower}.exe"
            "${OUTPUT_DIR}/${route}-${lower}.log" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "SH20 ${profile}/${route} link failed: ${out}${err}")
        endif()
    endforeach()
endforeach()
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_source_bool_void.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/source-bool-void.contract.json"
    --backend-source "${SOURCE_ROOT}/selfhost/nsc/CodeGen/LlvmBackend.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh20-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH20 verification failed: ${out}${err}")
endif()
message(STATUS "${out}")
