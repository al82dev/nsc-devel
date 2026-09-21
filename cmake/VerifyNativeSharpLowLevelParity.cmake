if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH45 requires Python, roots, SH41 products, LLD and NSLink")
endif()

get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-lld.exe")
set(llvm_nm "${llvm_bin}/llvm-nm.exe")
set(llvm_objdump "${llvm_bin}/llvm-objdump.exe")
foreach(required IN ITEMS PYTHON_EXECUTABLE candidate LINKER_EXECUTABLE NSLINK_EXECUTABLE llvm_nm llvm_objdump)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH45 missing ${required}: ${${required}}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_low_level_parity.py"
    --source-root "${SOURCE_ROOT}"
    --contract "${SOURCE_ROOT}/selfhost/contracts/low-level-parity.contract.json"
    --compiler "${candidate}" --nslink "${NSLINK_EXECUTABLE}"
    --lld-link "${LINKER_EXECUTABLE}" --llvm-nm "${llvm_nm}" --llvm-objdump "${llvm_objdump}"
    --output "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh45-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH45 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
