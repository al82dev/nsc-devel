if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED ENV{NS_ROOT})
    message(FATAL_ERROR "SH10 requires nsc0, Python, roots and linker")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(prefix "$ENV{NS_ROOT}")
set(nslink "${prefix}/bin/nslink.exe")
set(adapter_project "${SOURCE_ROOT}/selfhost/tests/llvm/LinkerAdapterSmoke/LinkerAdapterSmoke.nsproj")
set(execution_project "${SOURCE_ROOT}/selfhost/tests/projects/Sh09App/Sh09App.nsproj")
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

set(nsc1_a "${clean_a}/nsc1.exe")
set(nsc1_b "${clean_b}/nsc1.exe")
foreach(pair IN ITEMS "${clean_a}|${nsc1_a}" "${clean_b}|${nsc1_b}")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 clean_root)
    list(GET fields 1 candidate)
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
            "${clean_root}/selfhost/nsc/NativeSharp.Compiler.nsproj" -O2 -g0 -o "${candidate}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH10 clean nsc1 build failed: ${out}${err}")
    endif()
endforeach()
file(SHA256 "${nsc1_a}" hash_a)
file(SHA256 "${nsc1_b}" hash_b)
if(NOT hash_a STREQUAL hash_b)
    message(FATAL_ERROR "SH10 nsc1 clean builds are not byte-identical: ${hash_a} != ${hash_b}")
endif()

set(adapter "${OUTPUT_DIR}/adapter.exe")
execute_process(COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${adapter_project}" -O0 -g0 -o "${adapter}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH10 adapter build failed: ${out}${err}")
endif()
set(import_library "${OUTPUT_DIR}/kernel32.lib")
execute_process(COMMAND "${nslink}" --link --make-implib KERNEL32.dll:ExitProcess "${import_library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH10 import library failed: ${out}${err}")
endif()

foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(object "${OUTPUT_DIR}/corpus-${lower}.obj")
    execute_process(COMMAND "${nsc1_a}" --emit-project-object "${execution_project}" "-${profile}" -o "${object}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH10 nsc1 ${profile} corpus compile failed: ${out}${err}")
    endif()
    foreach(route IN ITEMS lld nslink)
        if(route STREQUAL "lld")
            set(tool "${LINKER_EXECUTABLE}")
        else()
            set(tool "${nslink}")
        endif()
        execute_process(COMMAND "${adapter}" "${route}" "${tool}" "${object}" "${import_library}"
            "${OUTPUT_DIR}/${route}-${lower}.rsp" "${OUTPUT_DIR}/${route}-${lower}.exe"
            "${OUTPUT_DIR}/${route}-${lower}.log"
            RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "SH10 ${profile}/${route} link failed: ${out}${err}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_first_generation.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/first-generation.contract.json"
        --source-root "${SOURCE_ROOT}"
        --clean-a-root "${clean_a}"
        --clean-b-root "${clean_b}"
        --nsc0 "${NSC_EXECUTABLE}"
        --nsc1-a "${nsc1_a}"
        --nsc1-b "${nsc1_b}"
        --linker "${LINKER_EXECUTABLE}"
        --nslink "${nslink}"
        --output-dir "${OUTPUT_DIR}"
        --report "${OUTPUT_DIR}/sh10-report.json"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH10 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
