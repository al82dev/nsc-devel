if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH11 requires nsc0, Python, roots and linker")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
set(clean_a "${OUTPUT_DIR}/clean-a")
set(clean_b "${OUTPUT_DIR}/clean-b")
file(REMOVE_RECURSE "${clean_a}" "${clean_b}")

foreach(clean_root IN ITEMS "${clean_a}" "${clean_b}")
    file(MAKE_DIRECTORY "${clean_root}/selfhost" "${clean_root}/lib")
    file(COPY "${SOURCE_ROOT}/selfhost/nsc" DESTINATION "${clean_root}/selfhost")
    foreach(library IN ITEMS NativeSharp.Core NativeSharp.Core.Collections.Generic NativeSharp.Core.IO NativeSharp.LLVM.C)
        file(COPY "${SOURCE_ROOT}/lib/${library}" DESTINATION "${clean_root}/lib")
    endforeach()
endforeach()

set(nsc1 "${OUTPUT_DIR}/nsc1.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${clean_a}/selfhost/nsc/NativeSharp.Compiler.nsproj" -O2 -g0 -o "${nsc1}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH11 nsc1 preparation failed: ${out}${err}")
endif()

set(adapter "${OUTPUT_DIR}/adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj"
    -O0 -g0 -o "${adapter}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH11 adapter build failed: ${out}${err}")
endif()

set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH11 import library failed: ${out}${err}")
endif()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_self_hosted_generation.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/self-hosted-generation.contract.json"
        --source-root "${SOURCE_ROOT}"
        --clean-a-root "${clean_a}"
        --clean-b-root "${clean_b}"
        --nsc0 "${NSC_EXECUTABLE}"
        --nsc1 "${nsc1}"
        --adapter "${adapter}"
        --linker "${LINKER_EXECUTABLE}"
        --nslink "${nslink}"
        --import-library "${import_library}"
        --output-dir "${OUTPUT_DIR}"
        --report "${OUTPUT_DIR}/sh11-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH11 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
