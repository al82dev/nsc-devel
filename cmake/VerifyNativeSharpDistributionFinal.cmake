if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED BOOTSTRAP_COMPILER OR NOT DEFINED BOOTSTRAP_LOCK OR
   NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED CMAKE_EXECUTABLE OR
   NOT DEFINED NINJA_EXECUTABLE OR NOT DEFINED CLANG_CL_EXECUTABLE OR
   NOT DEFINED LLD_LINK_EXECUTABLE OR NOT DEFINED LLVM_LIBRARY OR
   NOT DEFINED NSLINK_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED CTEST_EXECUTABLE OR NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "SH53 requires compiler, frozen toolchain, roots, SH41 products and CTest")
endif()
set(candidate "${SH41_OUTPUT_DIR}/nsc2-o0-lld.exe")
foreach(required IN ITEMS NSC_EXECUTABLE PYTHON_EXECUTABLE CMAKE_EXECUTABLE NINJA_EXECUTABLE
                          CLANG_CL_EXECUTABLE LLD_LINK_EXECUTABLE LLVM_LIBRARY NSLINK_EXECUTABLE
                          BOOTSTRAP_COMPILER BOOTSTRAP_LOCK candidate CTEST_EXECUTABLE)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH53 missing ${required}: ${${required}}")
    endif()
endforeach()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
get_filename_component(llvm_bin "${CLANG_CL_EXECUTABLE}" DIRECTORY)
get_filename_component(llvm_root "${llvm_bin}" DIRECTORY)
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_distribution_final.py"
    --source-root "${SOURCE_ROOT}"
    --contract "${SOURCE_ROOT}/selfhost/contracts/distribution-final.contract.json"
    --compiler "${NSC_EXECUTABLE}" --selfhost-compiler "${candidate}"
    --bootstrap-compiler "${BOOTSTRAP_COMPILER}" --bootstrap-lock "${BOOTSTRAP_LOCK}"
    --cmake "${CMAKE_EXECUTABLE}" --ninja "${NINJA_EXECUTABLE}"
    --llvm-root "${llvm_root}" --nslink "${NSLINK_EXECUTABLE}"
    --ctest "${CTEST_EXECUTABLE}" --build-dir "${BUILD_DIR}"
    --output "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh53-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH53 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
