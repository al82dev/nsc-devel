if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE)
    message(FATAL_ERROR "SH03 requires nsc, Python, source root, output dir and the LLVM tools directory")
endif()

if(NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH03 requires the configured NativeSharp prefix in NS_ROOT")
endif()
set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(llvm_readobj "${llvm_bin}/llvm-readobj.exe")
if(NOT EXISTS "${nslink}" OR NOT EXISTS "${llvm_readobj}")
    message(FATAL_ERROR "SH03 tools are absent: nslink=${nslink}; llvm-readobj=${llvm_readobj}")
endif()

set(space_dir "${OUTPUT_DIR}/path with spaces")
file(MAKE_DIRECTORY "${space_dir}")

set(llvm_contract_report "${OUTPUT_DIR}/llvm-c-contract-report.json")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B
        "${SOURCE_ROOT}/selfhost/bootstrap/verify_llvm_c_contract.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/llvm-c-api.contract.json"
        --source-root "${SOURCE_ROOT}"
        --nsc "${NSC_EXECUTABLE}"
        --llvm-readobj "${llvm_readobj}"
        --report "${llvm_contract_report}"
    RESULT_VARIABLE contract_result OUTPUT_VARIABLE contract_out ERROR_VARIABLE contract_err)
if(NOT contract_result EQUAL 0)
    message(FATAL_ERROR "SH03 LLVM-C contract failed:\n${contract_out}${contract_err}")
endif()

set(runtime_report "${OUTPUT_DIR}/runtime-bootstrap-report.json")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B
        "${SOURCE_ROOT}/selfhost/bootstrap/verify_runtime_bootstrap_contract.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/runtime-bootstrap.contract.json"
        --prefix "${prefix}"
        --nsc "${NSC_EXECUTABLE}"
        --report "${runtime_report}"
    RESULT_VARIABLE runtime_result OUTPUT_VARIABLE runtime_out ERROR_VARIABLE runtime_err)
if(NOT runtime_result EQUAL 0)
    message(FATAL_ERROR "SH03 runtime contract failed:\n${runtime_out}${runtime_err}")
endif()

set(emitter_project
    "${SOURCE_ROOT}/selfhost/tests/llvm/EmitMinimalObject/EmitMinimalObject.nsproj")
set(emitter "${OUTPUT_DIR}/emit-minimal-object.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${emitter_project}" -O0 -o "${emitter}"
    RESULT_VARIABLE emitter_compile OUTPUT_VARIABLE emitter_compile_out ERROR_VARIABLE emitter_compile_err)
if(NOT emitter_compile EQUAL 0)
    message(FATAL_ERROR "SH03 emitter compilation failed:\n${emitter_compile_out}${emitter_compile_err}")
endif()

execute_process(
    COMMAND "${emitter}" --reject-version ignored
    RESULT_VARIABLE reject_result OUTPUT_VARIABLE reject_out ERROR_VARIABLE reject_err)
if(NOT reject_result EQUAL 35 OR NOT reject_err MATCHES "NSH0301: unsupported loaded LLVM 22[.]0[.]0")
    message(FATAL_ERROR "SH03 incompatible LLVM policy was not stable: ${reject_result}\n${reject_out}${reject_err}")
endif()

set(object "${space_dir}/minimal object.obj")
execute_process(
    COMMAND "${emitter}" "${object}"
    RESULT_VARIABLE emit_result OUTPUT_VARIABLE emit_out ERROR_VARIABLE emit_err)
if(NOT emit_result EQUAL 42 OR NOT EXISTS "${object}" OR
   NOT emit_out MATCHES "SH03 LLVM 23 object emitted")
    message(FATAL_ERROR "SH03 NativeSharp LLVM emitter failed: ${emit_result}\n${emit_out}${emit_err}")
endif()

set(import_library "${space_dir}/kernel32 exit.lib")
execute_process(
    COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE implib_result OUTPUT_VARIABLE implib_out ERROR_VARIABLE implib_err)
if(NOT implib_result EQUAL 0 OR NOT EXISTS "${import_library}")
    message(FATAL_ERROR "SH03 import library generation failed:\n${implib_out}${implib_err}")
endif()

set(adapter_project
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj")
set(adapter "${OUTPUT_DIR}/linker-adapter-smoke.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${adapter_project}" -O0 -o "${adapter}"
    RESULT_VARIABLE adapter_compile OUTPUT_VARIABLE adapter_compile_out ERROR_VARIABLE adapter_compile_err)
if(NOT adapter_compile EQUAL 0)
    message(FATAL_ERROR "SH03 linker adapter compilation failed:\n${adapter_compile_out}${adapter_compile_err}")
endif()

foreach(route IN ITEMS nslink)
    set(route_linker "${nslink}")
    set(response "${space_dir}/${route} response file.rsp")
    set(executable "${space_dir}/${route} linked output.exe")
    set(link_log "${space_dir}/${route} linker output.log")
    execute_process(
        COMMAND "${adapter}" "${route}" "${route_linker}" "${object}" "${import_library}"
            "${response}" "${executable}" "${link_log}"
        RESULT_VARIABLE link_result OUTPUT_VARIABLE link_out ERROR_VARIABLE link_err)
    if(NOT link_result EQUAL 0 OR NOT EXISTS "${response}" OR NOT EXISTS "${executable}")
        message(FATAL_ERROR "SH03 ${route} adapter failed: ${link_result}\n${link_out}${link_err}")
    endif()
    execute_process(COMMAND "${executable}" RESULT_VARIABLE run_result)
    if(NOT run_result EQUAL 42)
        message(FATAL_ERROR "SH03 ${route} executable returned ${run_result}, expected 42")
    endif()
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --verify-pe "${executable}"
        RESULT_VARIABLE pe_result OUTPUT_VARIABLE pe_out ERROR_VARIABLE pe_err)
    if(NOT pe_result EQUAL 0 OR NOT pe_out MATCHES "imports=1")
        message(FATAL_ERROR "SH03 ${route} PE verification failed:\n${pe_out}${pe_err}")
    endif()
endforeach()

set(services_project
    "${SOURCE_ROOT}/selfhost/tests/llvm/CompilerServicesSmoke/CompilerServicesSmoke.nsproj")
set(services "${OUTPUT_DIR}/compiler-services-smoke.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${services_project}" -O0 -o "${services}"
    RESULT_VARIABLE services_compile OUTPUT_VARIABLE services_compile_out ERROR_VARIABLE services_compile_err)
if(NOT services_compile EQUAL 0)
    message(FATAL_ERROR "SH03 compiler services compilation failed:\n${services_compile_out}${services_compile_err}")
endif()
execute_process(
    COMMAND "${services}" "${space_dir}/mapped input.bin" "${nslink}"
        "${space_dir}/process output.log"
    RESULT_VARIABLE services_result OUTPUT_VARIABLE services_out ERROR_VARIABLE services_err)
if(NOT services_result EQUAL 42)
    message(FATAL_ERROR "SH03 compiler services returned ${services_result}:\n${services_out}${services_err}")
endif()

file(SHA256 "${object}" object_sha256)
file(SHA256 "${space_dir}/nslink linked output.exe" nslink_sha256)
file(WRITE "${OUTPUT_DIR}/sh03-report.json"
    "{\n"
    "  \"schema\": \"nativesharp-selfhost-sh03-report\",\n"
    "  \"version\": 1,\n"
    "  \"status\": \"passed\",\n"
    "  \"llvm_major\": 23,\n"
    "  \"llvm_linkage\": \"static-components\",\n"
    "  \"llvm_c_dynamic_imports\": 0,\n"
    "  \"bootstrap_llvm_symbols\": 42,\n"
    "  \"linkers\": [\"NSLink\"],\n"
    "  \"paths_with_spaces\": true,\n"
    "  \"response_files\": true,\n"
    "  \"expected_exit_code\": 42,\n"
    "  \"object_sha256\": \"${object_sha256}\",\n"
    "  \"nslink_executable_sha256\": \"${nslink_sha256}\"\n"
    "}\n")
