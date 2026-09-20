if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE)
    message(FATAL_ERROR "SH16 requires nsc0, Python, roots and lld-link")
endif()
if(NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH16 requires NS_ROOT")
endif()

set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(adapter "${OUTPUT_DIR}/linker-adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj"
    -O0 -g0 -o "${adapter}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH16 adapter build failed: ${out}${err}")
endif()
set(import_library "${OUTPUT_DIR}/kernel32-exit.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH16 import library failed: ${out}${err}")
endif()

foreach(profile IN ITEMS O0 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH16 ${profile} compiler build failed: ${out}${err}")
    endif()
    foreach(expected IN ITEMS 17 42)
        set(source "${SOURCE_ROOT}/selfhost/tests/codegen/source-derived-${expected}.ns")
        set(object "${OUTPUT_DIR}/source-${expected}-${lower}.obj")
        execute_process(COMMAND "${candidate}" --emit-object "${source}" "-${profile}" -o "${object}"
            RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT result EQUAL 0 OR NOT EXISTS "${object}")
            message(FATAL_ERROR "SH16 ${profile}/${expected} emission failed: ${out}${err}")
        endif()
        foreach(route IN ITEMS lld nslink)
            if(route STREQUAL "lld")
                set(route_linker "${LINKER_EXECUTABLE}")
            else()
                set(route_linker "${nslink}")
            endif()
            set(response "${OUTPUT_DIR}/${route}-${expected}-${lower}.rsp")
            set(executable "${OUTPUT_DIR}/${route}-${expected}-${lower}.exe")
            set(link_log "${OUTPUT_DIR}/${route}-${expected}-${lower}.log")
            execute_process(COMMAND "${adapter}" "${route}" "${route_linker}" "${object}"
                "${import_library}" "${response}" "${executable}" "${link_log}"
                RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
            if(NOT result EQUAL 0 OR NOT EXISTS "${executable}")
                message(FATAL_ERROR "SH16 ${profile}/${route}/${expected} link failed: ${out}${err}")
            endif()
        endforeach()
    endforeach()
endforeach()

execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_source_derived_backend.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/source-derived-backend.contract.json"
    --backend-source "${SOURCE_ROOT}/selfhost/nsc/CodeGen/LlvmBackend.ns"
    --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh16-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH16 verification failed: ${out}${err}")
endif()
message(STATUS "${out}")
