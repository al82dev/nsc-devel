if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED SH41_OUTPUT_DIR OR
   NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH42 requires nsc0, Python, roots, SH41 products and NSLink")
endif()

foreach(required IN ITEMS NSC_EXECUTABLE PYTHON_EXECUTABLE NSLINK_EXECUTABLE)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH42 missing ${required}: ${${required}}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(candidate_arguments)
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${SH41_OUTPUT_DIR}/nsc2-${lower}-nslink.exe")
    if(NOT EXISTS "${candidate}")
        message(FATAL_ERROR "SH42 missing SH41 nsc2 image: ${candidate}")
    endif()
    list(APPEND candidate_arguments "--nsc2-${lower}" "${candidate}")
endforeach()

set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(
    COMMAND "${NSLINK_EXECUTABLE}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH42 import library failed: ${out}${err}")
endif()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B
        "${SOURCE_ROOT}/selfhost/bootstrap/verify_functional_equivalence.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/functional-equivalence.contract.json"
        --source-root "${SOURCE_ROOT}" --python "${PYTHON_EXECUTABLE}"
        --nsc0 "${NSC_EXECUTABLE}" ${candidate_arguments}
        --nslink "${NSLINK_EXECUTABLE}"
        --import-library "${import_library}" --output-dir "${OUTPUT_DIR}"
        --report "${OUTPUT_DIR}/sh42-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH42 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
