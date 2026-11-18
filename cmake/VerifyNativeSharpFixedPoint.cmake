if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH12 requires nsc0, Python, roots and NSLink")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/NativeSharpCleanRootLibraries.cmake")
nsc_clean_root_libraries_source("${SOURCE_ROOT}" libraries_root)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
set(clean_roots "${OUTPUT_DIR}/clean-a" "${OUTPUT_DIR}/clean-b" "${OUTPUT_DIR}/clean-c")
file(REMOVE_RECURSE ${clean_roots})
foreach(clean_root IN LISTS clean_roots)
    nsc_make_clean_root("${SOURCE_ROOT}" "${libraries_root}" "${clean_root}"
        NativeSharp.Core NativeSharp.Core.Collections.Generic NativeSharp.Core.IO NativeSharp.LLVM.C)
endforeach()
list(GET clean_roots 0 clean_a)
set(nsc1 "${OUTPUT_DIR}/nsc1.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${clean_a}/compiler/selfhost/nsc/NativeSharp.Compiler.nsproj" -O2 -g0 -o "${nsc1}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH12 nsc1 preparation failed: ${out}${err}")
endif()
set(adapter "${OUTPUT_DIR}/adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
    "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj" -O0 -g0 -o "${adapter}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH12 adapter failed: ${out}${err}")
endif()
set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH12 import library failed: ${out}${err}")
endif()
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_fixed_point.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/fixed-point.contract.json"
        --source-root "${SOURCE_ROOT}"
        --clean-root "${OUTPUT_DIR}/clean-a/compiler" --clean-root "${OUTPUT_DIR}/clean-b/compiler"
        --clean-root "${OUTPUT_DIR}/clean-c/compiler"
        --nsc1 "${nsc1}" --adapter "${adapter}" --nslink "${nslink}"
        --import-library "${import_library}" --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh12-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH12 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
