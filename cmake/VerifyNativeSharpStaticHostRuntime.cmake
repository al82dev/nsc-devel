if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the linked nsc.exe")
endif()
if(NOT DEFINED LLVM_READOBJ OR NOT EXISTS "${LLVM_READOBJ}")
    message(FATAL_ERROR "LLVM_READOBJ must name llvm-readobj.exe")
endif()

execute_process(
    COMMAND "${LLVM_READOBJ}" --coff-imports "${NSC_EXECUTABLE}"
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE imports
    ERROR_VARIABLE inspect_error)
if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR
        "could not inspect nsc.exe imports (${inspect_result}): ${inspect_error}")
endif()

string(TOLOWER "${imports}" imports_lower)
set(forbidden_patterns
    "msvcp[0-9a-z_]*\\.dll"
    "vcruntime[0-9a-z_]*\\.dll"
    "ucrtbased.dll"
    "ucrtbase.dll"
    "api-ms-win-crt-[0-9a-z_-]*\\.dll")
if(NOT NSC_ALLOW_LLVM_C)
    list(PREPEND forbidden_patterns "llvm-c.dll")
endif()
foreach(pattern IN LISTS forbidden_patterns)
    if(imports_lower MATCHES "${pattern}")
        message(FATAL_ERROR
            "nsc.exe is not self-contained: forbidden runtime import matches '${pattern}'")
    endif()
endforeach()

if(NSC_ALLOW_LLVM_C)
    if(NOT imports_lower MATCHES "llvm-c.dll")
        message(FATAL_ERROR "dynamic development nsc.exe does not import LLVM-C.dll")
    endif()
    message(STATUS "nsc.exe dynamic LLVM/static MSVC runtime contract: passed")
else()
    message(STATUS "nsc.exe static host-runtime contract: passed")
endif()
