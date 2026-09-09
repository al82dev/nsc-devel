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

set(line_executable "${DEBUG_OUTPUT_DIR}/debug_lines.exe")
set(line_pdb "${DEBUG_OUTPUT_DIR}/debug_lines.pdb")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${DEBUG_FIXTURE}"
        -gline-tables-only -O0 -o "${line_executable}"
    RESULT_VARIABLE line_executable_result
    OUTPUT_VARIABLE line_executable_output
    ERROR_VARIABLE line_executable_error)
if(NOT line_executable_result EQUAL 0 OR
   NOT EXISTS "${line_executable}" OR NOT EXISTS "${line_pdb}")
    message(FATAL_ERROR
        "line-tables-only executable/PDB compilation failed (${line_executable_result})\n"
        "stdout:\n${line_executable_output}\nstderr:\n${line_executable_error}")
endif()
execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -summary -symbols --modi=0 "${line_pdb}"
    RESULT_VARIABLE line_pdb_result
    OUTPUT_VARIABLE line_pdb_dump
    ERROR_VARIABLE line_pdb_error)
if(NOT line_pdb_result EQUAL 0 OR
   NOT line_pdb_dump MATCHES "Has Debug Info: true" OR
   NOT line_pdb_dump MATCHES "DebugFixture\\.Program\\.Compute" OR
   NOT line_pdb_dump MATCHES "DebugFixture\\.Program\\.Main" OR
   line_pdb_dump MATCHES "S_LOCAL" OR line_pdb_dump MATCHES "S_DEFRANGE")
    message(FATAL_ERROR
        "line-tables-only PDB did not preserve the line-only profile boundary\n"
        "${line_pdb_dump}\n${line_pdb_error}")
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
        COMMAND "${executable}"
        RESULT_VARIABLE execution_result
        OUTPUT_VARIABLE execution_output
        ERROR_VARIABLE execution_error)
    if(NOT execution_result EQUAL 42)
        message(FATAL_ERROR
            "${level} debug fixture changed behavior (${execution_result}, expected 42)\n"
            "stdout:\n${execution_output}\nstderr:\n${execution_error}")
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
        COMMAND "${LLVM_PDBUTIL}" dump -summary -modules -files -types -symbols --modi=0 "${pdb}"
        RESULT_VARIABLE pdb_result
        OUTPUT_VARIABLE pdb_dump
        ERROR_VARIABLE pdb_error)
    if(NOT pdb_result EQUAL 0 OR
       NOT pdb_dump MATCHES "Has Debug Info: true" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Pair\\.Total" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Compute" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Sum" OR
       NOT pdb_dump MATCHES "S_THUNK32[^\n]*`[^`]*Increment[^`]*__c_boundary`" OR
       NOT pdb_dump MATCHES "S_THUNK32[^\n]*`ExportedIdentity`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.GeneratedState\\.cctor \\[generated\\]`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Resource\\.destruction-entry \\[generated\\]`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.DebugProblem\\.catch-filter \\[generated\\]`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Program\\.Main\\.lambda#0 \\[generated: closure context destroy\\]`" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Main" OR
       NOT pdb_dump MATCHES "Main\\.ns" OR
       NOT pdb_dump MATCHES "S_LOCAL[^\n]*`left`" OR
       NOT pdb_dump MATCHES "S_LOCAL[^\n]*`right`" OR
       NOT pdb_dump MATCHES "S_LOCAL[^\n]*`total`" OR
       NOT pdb_dump MATCHES "(S_LOCAL|S_CONSTANT)[^\n]*`answer`" OR
       NOT pdb_dump MATCHES "S_LOCAL[^\n]*`pair`" OR
       NOT pdb_dump MATCHES "S_LOCAL[^\n]*`this`" OR
       NOT pdb_dump MATCHES "S_BLOCK32" OR
       NOT pdb_dump MATCHES "type=0x0074 \\(int\\), flags = param" OR
       NOT pdb_dump MATCHES "LF_STRUCTURE[^\n]*DebugFixture\\.Pair" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Left`[^\n]*offset = 0[^\n]*attrs = public" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Right`[^\n]*offset = 4[^\n]*attrs = public" OR
       NOT pdb_dump MATCHES "sizeof 8" OR
       NOT pdb_dump MATCHES "int \\(DebugFixture\\.Pair\\)" OR
       NOT pdb_dump MATCHES "S_DEFRANGE")
        message(FATAL_ERROR
            "${level} PDB is missing debug summary, functions, variables, types or ranges\n"
            "${pdb_dump}\n${pdb_error}")
    endif()
    if(level STREQUAL "O0" AND
       (NOT pdb_dump MATCHES "S_BLOCK32[^\n]*``[\n\r ]+parent = [0-9]+, end = [0-9]+[\n\r ]+code size = [0-9]+, addr = [^\n]+[\n\r ]+[^\n]*S_LOCAL[^\n]*`scoped`"))
        message(FATAL_ERROR
            "O0 PDB is missing the nested lexical scope and its local\n"
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
       NOT lines_dump MATCHES "[ \n]45 [0-9A-F]+" OR
       NOT lines_dump MATCHES "[ \n]105 [0-9A-F]+")
        set(lines_valid FALSE)
    endif()
    if(NOT lines_valid)
        message(FATAL_ERROR
            "${level} PDB is missing expected executable Main.ns source lines\n"
            "${lines_dump}\n${lines_error}")
    endif()
endforeach()

message(STATUS "NativeSharp O0/O2 CodeView and PDB artifacts verified")
