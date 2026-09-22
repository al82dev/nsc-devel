if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED NSC_EXECUTABLE OR
   NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH48 requires Python, roots, seed, SH41 products, LLD and NSLink")
endif()

get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
get_filename_component(nslink_bin "${NSLINK_EXECUTABLE}" DIRECTORY)
get_filename_component(prefix "${nslink_bin}" DIRECTORY)
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-lld.exe")
set(runtime_library "${prefix}/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
set(runtime_abi "${prefix}/share/nativesharp/runtime/NativeSharpRuntime.abi")
set(llvm_ar "${llvm_bin}/llvm-ar.exe")
set(llvm_nm "${llvm_bin}/llvm-nm.exe")
set(import_arguments)
foreach(name IN ITEMS kernel32 kernelbase shell32 ucrtbase)
    set(path "${SH41_OUTPUT_DIR}/${name}.lib")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "SH48 missing SH41 import library: ${path}")
    endif()
    list(APPEND import_arguments --import-library "${path}")
endforeach()
foreach(required IN ITEMS PYTHON_EXECUTABLE NSC_EXECUTABLE candidate runtime_library runtime_abi
                          LINKER_EXECUTABLE NSLINK_EXECUTABLE llvm_ar llvm_nm)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH48 missing ${required}: ${${required}}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_runtime_library.py"
    --source-root "${SOURCE_ROOT}"
    --contract "${SOURCE_ROOT}/selfhost/contracts/runtime-library.contract.json"
    --compiler "${candidate}" --seed-compiler "${NSC_EXECUTABLE}"
    --runtime-library "${runtime_library}" --runtime-abi "${runtime_abi}"
    --lld-link "${LINKER_EXECUTABLE}" --nslink "${NSLINK_EXECUTABLE}"
    --llvm-ar "${llvm_ar}" --llvm-nm "${llvm_nm}" ${import_arguments}
    --output "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh48-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH48 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
