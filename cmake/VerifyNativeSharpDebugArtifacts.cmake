if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED LLVM_PDBUTIL OR NOT EXISTS "${LLVM_PDBUTIL}")
    message(FATAL_ERROR "LLVM_PDBUTIL must name llvm-pdbutil")
endif()
if(NOT DEFINED LLVM_READOBJ OR NOT EXISTS "${LLVM_READOBJ}")
    message(FATAL_ERROR "LLVM_READOBJ must name llvm-readobj")
endif()
if(NOT DEFINED DEBUG_FIXTURE OR NOT EXISTS "${DEBUG_FIXTURE}")
    message(FATAL_ERROR "DEBUG_FIXTURE must name DebugPdb.nsproj")
endif()
if(NOT DEFINED DEBUG_OUTPUT_DIR)
    message(FATAL_ERROR "DEBUG_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${DEBUG_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEBUG_OUTPUT_DIR}")

set(line_object "${DEBUG_OUTPUT_DIR}/debug_lines.obj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${DEBUG_FIXTURE}"
        -gline-tables-only -O0 -o "${line_object}"
    RESULT_VARIABLE object_result
    OUTPUT_VARIABLE object_output
    ERROR_VARIABLE object_error)
if(NOT object_result EQUAL 0 OR NOT EXISTS "${line_object}")
    message(FATAL_ERROR
        "line-tables-only object compilation failed (${object_result})\n"
        "stdout:\n${object_output}\nstderr:\n${object_error}")
endif()
execute_process(
    COMMAND "${LLVM_READOBJ}" --sections "${line_object}"
    RESULT_VARIABLE sections_result
    OUTPUT_VARIABLE sections_dump
    ERROR_VARIABLE sections_error)
string(FIND "${sections_dump}" ".debug$S" debug_s_index)
if(NOT sections_result EQUAL 0 OR debug_s_index EQUAL -1)
    message(FATAL_ERROR
        "line-tables-only COFF object has no .debug$S section\n"
        "${sections_dump}\n${sections_error}")
endif()

foreach(level IN ITEMS O0 O2)
    string(TOLOWER "${level}" stem)
    set(executable "${DEBUG_OUTPUT_DIR}/debug_${stem}.exe")
    set(pdb "${DEBUG_OUTPUT_DIR}/debug_${stem}.pdb")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${DEBUG_FIXTURE}"
            -g0 -g "-${level}" -o "${executable}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error)
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR
            "${level} debug fixture compilation failed (${compile_result})\n"
            "stdout:\n${compile_output}\nstderr:\n${compile_error}")
    endif()
    if(NOT EXISTS "${executable}" OR NOT EXISTS "${pdb}")
        message(FATAL_ERROR "${level} did not create both debug_${stem}.exe and debug_${stem}.pdb")
    endif()

    execute_process(
        COMMAND "${LLVM_READOBJ}" --coff-debug-directory "${executable}"
        RESULT_VARIABLE readobj_result
        OUTPUT_VARIABLE debug_directory
        ERROR_VARIABLE readobj_error)
    if(NOT readobj_result EQUAL 0 OR
       NOT debug_directory MATCHES "Type: CodeView" OR
       NOT debug_directory MATCHES "PDBSignature: 0x53445352" OR
       NOT debug_directory MATCHES "PDBFileName: debug_${stem}\\.pdb")
        message(FATAL_ERROR
            "${level} PE has no matching basename-only RSDS record\n"
            "${debug_directory}\n${readobj_error}")
    endif()

    execute_process(
        COMMAND "${LLVM_PDBUTIL}" dump -summary -modules -files -symbols --modi=0 "${pdb}"
        RESULT_VARIABLE pdb_result
        OUTPUT_VARIABLE pdb_dump
        ERROR_VARIABLE pdb_error)
    if(NOT pdb_result EQUAL 0 OR
       NOT pdb_dump MATCHES "Has Debug Info: true" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Main" OR
       NOT pdb_dump MATCHES "Main\\.ns")
        message(FATAL_ERROR
            "${level} PDB is missing debug summary, function or source file\n"
            "${pdb_dump}\n${pdb_error}")
    endif()

    execute_process(
        COMMAND "${LLVM_PDBUTIL}" dump -l --modi=0 "${pdb}"
        RESULT_VARIABLE lines_result
        OUTPUT_VARIABLE lines_dump
        ERROR_VARIABLE lines_error)
    set(lines_valid TRUE)
    if(NOT lines_result EQUAL 0 OR
       NOT lines_dump MATCHES "Main\\.ns" OR
       NOT lines_dump MATCHES "[ \n]9 [0-9A-F]+")
        set(lines_valid FALSE)
    endif()
    if(level STREQUAL "O0" AND NOT lines_dump MATCHES "[ \n]7 [0-9A-F]+")
        set(lines_valid FALSE)
    endif()
    if(NOT lines_valid)
        message(FATAL_ERROR
            "${level} PDB is missing expected executable Main.ns source lines\n"
            "${lines_dump}\n${lines_error}")
    endif()
endforeach()

message(STATUS "NativeSharp O0/O2 CodeView and PDB artifacts verified")
