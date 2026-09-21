if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH46 requires Python, roots, SH41 products, LLD and NSLink")
endif()

get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-lld.exe")
set(clang_cl "${llvm_bin}/clang-cl.exe")
set(llvm_nm "${llvm_bin}/llvm-nm.exe")
foreach(required IN ITEMS PYTHON_EXECUTABLE candidate LINKER_EXECUTABLE NSLINK_EXECUTABLE clang_cl llvm_nm)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH46 missing ${required}: ${${required}}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_interop_parity.py"
    --source-root "${SOURCE_ROOT}"
    --contract "${SOURCE_ROOT}/selfhost/contracts/interop-parity.contract.json"
    --compiler "${candidate}" --nslink "${NSLINK_EXECUTABLE}"
    --lld-link "${LINKER_EXECUTABLE}" --clang-cl "${clang_cl}" --llvm-nm "${llvm_nm}"
    --output "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh46-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH46 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
