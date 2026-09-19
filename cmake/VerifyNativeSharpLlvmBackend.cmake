if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE)
    message(FATAL_ERROR "SH08 requires nsc, Python, source root, output dir and lld-link")
endif()
if(NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH08 requires the configured NativeSharp prefix in NS_ROOT")
endif()

set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(llvm_readobj "${llvm_bin}/llvm-readobj.exe")
if(NOT EXISTS "${nslink}" OR NOT EXISTS "${llvm_readobj}")
    message(FATAL_ERROR "SH08 tools are absent: nslink=${nslink}; llvm-readobj=${llvm_readobj}")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(adapter_project "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj")
set(adapter "${OUTPUT_DIR}/linker-adapter.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${adapter_project}" -O0 -g0 -o "${adapter}"
    RESULT_VARIABLE adapter_result OUTPUT_VARIABLE adapter_out ERROR_VARIABLE adapter_err)
if(NOT adapter_result EQUAL 0)
    message(FATAL_ERROR "SH08 linker adapter compilation failed:\n${adapter_out}${adapter_err}")
endif()

set(import_library "${OUTPUT_DIR}/kernel32-exit.lib")
execute_process(
    COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE implib_result OUTPUT_VARIABLE implib_out ERROR_VARIABLE implib_err)
if(NOT implib_result EQUAL 0)
    message(FATAL_ERROR "SH08 import library generation failed:\n${implib_out}${implib_err}")
endif()

set(project "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj")
set(source "${SOURCE_ROOT}/tests/integration/valid/full_frontend.ns")
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${OUTPUT_DIR}/nsc-next-${lower}.exe")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${project}" "-${profile}" -g0 -o "${candidate}"
        RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err)
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "SH08 ${profile} compiler build failed:\n${compile_out}${compile_err}")
    endif()

    set(object "${OUTPUT_DIR}/backend-${lower}.obj")
    set(assembly "${OUTPUT_DIR}/backend-${lower}.asm")
    execute_process(
        COMMAND "${candidate}" --emit-object "${source}" "-${profile}" -o "${object}"
        RESULT_VARIABLE object_result OUTPUT_VARIABLE object_out ERROR_VARIABLE object_err)
    if(NOT object_result EQUAL 0 OR NOT EXISTS "${object}")
        message(FATAL_ERROR "SH08 ${profile} object emission failed:\n${object_out}${object_err}")
    endif()
    execute_process(
        COMMAND "${candidate}" --emit-assembly "${source}" "-${profile}" -o "${assembly}"
        RESULT_VARIABLE assembly_result OUTPUT_VARIABLE assembly_out ERROR_VARIABLE assembly_err)
    if(NOT assembly_result EQUAL 0 OR NOT EXISTS "${assembly}")
        message(FATAL_ERROR "SH08 ${profile} assembly emission failed:\n${assembly_out}${assembly_err}")
    endif()
    execute_process(
        COMMAND "${llvm_readobj}" --file-headers --sections --symbols --relocations --unwind "${object}"
        OUTPUT_FILE "${OUTPUT_DIR}/backend-${lower}.readobj.txt"
        RESULT_VARIABLE readobj_result ERROR_VARIABLE readobj_err)
    if(NOT readobj_result EQUAL 0)
        message(FATAL_ERROR "SH08 ${profile} COFF inspection failed:\n${readobj_err}")
    endif()

    foreach(route IN ITEMS lld nslink)
        if(route STREQUAL "lld")
            set(route_linker "${LINKER_EXECUTABLE}")
        else()
            set(route_linker "${nslink}")
        endif()
        set(response "${OUTPUT_DIR}/${route}-${lower}.rsp")
        set(executable "${OUTPUT_DIR}/${route}-${lower}.exe")
        set(link_log "${OUTPUT_DIR}/${route}-${lower}.log")
        execute_process(
            COMMAND "${adapter}" "${route}" "${route_linker}" "${object}" "${import_library}"
                "${response}" "${executable}" "${link_log}"
            RESULT_VARIABLE link_result OUTPUT_VARIABLE link_out ERROR_VARIABLE link_err)
        if(NOT link_result EQUAL 0 OR NOT EXISTS "${executable}")
            message(FATAL_ERROR "SH08 ${profile}/${route} link failed:\n${link_out}${link_err}")
        endif()
        execute_process(COMMAND "${executable}" RESULT_VARIABLE run_result)
        if(NOT run_result EQUAL 42)
            message(FATAL_ERROR "SH08 ${profile}/${route} returned ${run_result}, expected 42")
        endif()
        execute_process(
            COMMAND "${NSC_EXECUTABLE}" --verify-pe "${executable}"
            RESULT_VARIABLE pe_result OUTPUT_VARIABLE pe_out ERROR_VARIABLE pe_err)
        if(NOT pe_result EQUAL 0 OR NOT pe_out MATCHES "imports=1")
            message(FATAL_ERROR "SH08 ${profile}/${route} PE verification failed:\n${pe_out}${pe_err}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_llvm_backend.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/llvm-backend.contract.json"
        --backend-source "${SOURCE_ROOT}/selfhost/nsc/CodeGen/LlvmBackend.ns"
        --output-dir "${OUTPUT_DIR}"
        --report "${OUTPUT_DIR}/sh08-report.json"
    RESULT_VARIABLE report_result OUTPUT_VARIABLE report_out ERROR_VARIABLE report_err)
if(NOT report_result EQUAL 0)
    message(FATAL_ERROR "SH08 report verification failed:\n${report_out}${report_err}")
endif()
message(STATUS "${report_out}")
