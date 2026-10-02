if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR
   NOT DEFINED NSLINK_EXECUTABLE OR NOT DEFINED NSASM_EXECUTABLE)
    message(FATAL_ERROR "SH47 requires Python, roots, SH41 products, LLVM tools, NSLink and NSAsm")
endif()

get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-nslink.exe")
set(llvm_objdump "${llvm_bin}/llvm-objdump.exe")
foreach(required IN ITEMS PYTHON_EXECUTABLE candidate LINKER_EXECUTABLE NSLINK_EXECUTABLE
                          NSASM_EXECUTABLE llvm_objdump)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH47 missing ${required}: ${${required}}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_specialized_code.py"
    --source-root "${SOURCE_ROOT}"
    --contract "${SOURCE_ROOT}/selfhost/contracts/specialized-code.contract.json"
    --compiler "${candidate}" --nsasm "${NSASM_EXECUTABLE}"
    --nslink "${NSLINK_EXECUTABLE}"
    --llvm-objdump "${llvm_objdump}" --output "${OUTPUT_DIR}"
    --report "${OUTPUT_DIR}/sh47-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH47 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
