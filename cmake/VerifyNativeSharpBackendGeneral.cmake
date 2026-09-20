if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH27 requires nsc0, Python, roots and linkers")
endif()
set(nslink "${NSLINK_EXECUTABLE}")
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(llvm_readobj "${llvm_bin}/llvm-readobj.exe")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(adapter "${OUTPUT_DIR}/adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj"
    -O0 -g0 -o "${adapter}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH27 adapter failed: ${out}${err}")
endif()
set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH27 implib failed: ${out}${err}")
endif()
set(project "${SOURCE_ROOT}/selfhost/tests/backend-general/BackendGeneral.nsproj")
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH27 ${profile} compiler failed: ${out}${err}")
    endif()
    set(object "${OUTPUT_DIR}/backend-${lower}.obj")
    execute_process(COMMAND "${candidate}" --emit-project-object "${project}" "-${profile}" -o "${object}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0 OR NOT EXISTS "${object}")
        message(FATAL_ERROR "SH27 ${profile} project object failed: ${out}${err}")
    endif()
    execute_process(COMMAND "${llvm_readobj}" --symbols --relocations "${object}"
        OUTPUT_FILE "${OUTPUT_DIR}/backend-${lower}.readobj.txt" RESULT_VARIABLE result ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH27 ${profile} inspection failed: ${err}")
    endif()
    foreach(route IN ITEMS lld nslink)
        set(tool "${nslink}")
        if(route STREQUAL "lld")
            set(tool "${LINKER_EXECUTABLE}")
        endif()
        set(executable "${OUTPUT_DIR}/${route}-${lower}.exe")
        execute_process(COMMAND "${adapter}" "${route}" "${tool}" "${object}" "${import_library}"
            "${OUTPUT_DIR}/${route}-${lower}.rsp" "${executable}" "${OUTPUT_DIR}/${route}-${lower}.log"
            RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "SH27 ${profile}/${route} link failed: ${out}${err}")
        endif()
        execute_process(COMMAND "${executable}" RESULT_VARIABLE run)
        file(WRITE "${OUTPUT_DIR}/${route}-${lower}.result.txt" "${run}\n")
    endforeach()
endforeach()
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_backend_general.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/backend-general.contract.json"
    --backend-source "${SOURCE_ROOT}/selfhost/nsc/CodeGen/LlvmBackend.ns"
    --driver-source "${SOURCE_ROOT}/selfhost/nsc/Driver/Program.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh27-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH27 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
