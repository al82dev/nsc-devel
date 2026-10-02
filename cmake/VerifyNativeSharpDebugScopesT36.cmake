if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED LLVM_PDBUTIL OR NOT EXISTS "${LLVM_PDBUTIL}")
    message(FATAL_ERROR "LLVM_PDBUTIL must name llvm-pdbutil")
endif()
if(NOT DEFINED NSLINK_EXECUTABLE OR NOT EXISTS "${NSLINK_EXECUTABLE}")
    message(FATAL_ERROR "NSLINK_EXECUTABLE must name NSLink")
endif()
if(NOT DEFINED DEBUG_T36_FIXTURE OR NOT EXISTS "${DEBUG_T36_FIXTURE}")
    message(FATAL_ERROR "DEBUG_T36_FIXTURE must name DebugScopesT36.nsproj")
endif()
if(NOT DEFINED DEBUG_T36_OUTPUT_DIR)
    message(FATAL_ERROR "DEBUG_T36_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${DEBUG_T36_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEBUG_T36_OUTPUT_DIR}")
set(object "${DEBUG_T36_OUTPUT_DIR}/debug_scopes_t36.obj")
set(executable "${DEBUG_T36_OUTPUT_DIR}/debug_scopes_t36.exe")
set(pdb "${DEBUG_T36_OUTPUT_DIR}/debug_scopes_t36.pdb")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${DEBUG_T36_FIXTURE}"
        -g -O0 -o "${object}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
if(NOT compile_result EQUAL 0 OR NOT EXISTS "${object}")
    message(FATAL_ERROR
        "T36 O0 CodeView object compilation failed (${compile_result})\n"
        "stdout:\n${compile_output}\nstderr:\n${compile_error}")
endif()

execute_process(
    COMMAND "${NSLINK_EXECUTABLE}"
        /nodefaultlib /subsystem:console
        "/entry:__nsc_function_DebugScopesT36$3A$3ADebugScopesT36__Main"
        /debug:full "/pdbaltpath:%_PDB%"
        "/out:${executable}" "/pdb:${pdb}" "${object}"
    RESULT_VARIABLE link_result
    OUTPUT_VARIABLE link_output
    ERROR_VARIABLE link_error)
if(NOT link_result EQUAL 0 OR NOT EXISTS "${executable}" OR NOT EXISTS "${pdb}")
    message(FATAL_ERROR
        "T36 self-contained CodeView link failed (${link_result})\n"
        "stdout:\n${link_output}\nstderr:\n${link_error}")
endif()

execute_process(
    COMMAND "${executable}"
    RESULT_VARIABLE execution_result
    OUTPUT_VARIABLE execution_output
    ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 42)
    message(FATAL_ERROR
        "T36 fixture changed behavior (${execution_result}, expected 42)\n"
        "stdout:\n${execution_output}\nstderr:\n${execution_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -summary -symbols "${pdb}"
    RESULT_VARIABLE symbols_result
    OUTPUT_VARIABLE symbols_dump
    ERROR_VARIABLE symbols_error)
string(REGEX MATCHALL "S_LOCAL[^\n]*`i`" loop_locals "${symbols_dump}")
list(LENGTH loop_locals loop_local_count)
string(FIND "${symbols_dump}" "range = [0001:0004,+143)" total_range)
string(FIND "${symbols_dump}" "range = [0001:0023,+20)" late_range)
string(FIND "${symbols_dump}" "range = [0001:0043,+38)" first_loop_range)
string(FIND "${symbols_dump}" "range = [0001:0083,+37)" second_loop_range)
string(FIND "${symbols_dump}" "range = [0001:0122,+25)" next_range)
if(NOT symbols_result EQUAL 0 OR
   NOT symbols_dump MATCHES "Has Debug Info: true" OR
   NOT symbols_dump MATCHES "S_GPROC32[^\n]*`DebugScopesT36\\.Main`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`total`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`late`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`Next`" OR
   total_range EQUAL -1 OR late_range EQUAL -1 OR
   first_loop_range EQUAL -1 OR second_loop_range EQUAL -1 OR next_range EQUAL -1 OR
   NOT loop_local_count EQUAL 2)
    message(FATAL_ERROR
        "T36 PDB does not preserve declaration-bounded and non-overlapping lexical ranges\n"
        "${symbols_dump}\n${symbols_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -l "${pdb}"
    RESULT_VARIABLE lines_result
    OUTPUT_VARIABLE lines_dump
    ERROR_VARIABLE lines_error)
foreach(required_line 7 8 9 11 13 15 17 19 20 21)
    if(NOT lines_dump MATCHES "[ \n]${required_line} [0-9A-F]+")
        message(FATAL_ERROR
            "T36 executable statement on source line ${required_line} has no breakpoint site\n"
            "${lines_dump}\n${lines_error}")
    endif()
endforeach()
if(NOT lines_result EQUAL 0)
    message(FATAL_ERROR "T36 PDB line inspection failed\n${lines_dump}\n${lines_error}")
endif()

message(STATUS
    "NativeSharp T36 O0 lexical scopes, shadowing and breakpoint sites verified")
