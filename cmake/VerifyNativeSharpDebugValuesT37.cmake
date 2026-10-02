foreach(required NSC_EXECUTABLE LLVM_PDBUTIL LLVM_READOBJ LLVM_OBJDUMP NSLINK_EXECUTABLE
                 RUNTIME_LIBRARY NSIMPORTS_FILE DEBUG_T37_FIXTURE DEBUG_T37_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        if(required STREQUAL "DEBUG_T37_OUTPUT_DIR")
            if(DEFINED DEBUG_T37_OUTPUT_DIR)
                continue()
            endif()
        endif()
        message(FATAL_ERROR "${required} must name an existing T37 input")
    endif()
endforeach()

file(REMOVE_RECURSE "${DEBUG_T37_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEBUG_T37_OUTPUT_DIR}")
set(object "${DEBUG_T37_OUTPUT_DIR}/debug_values_t37.obj")
set(executable "${DEBUG_T37_OUTPUT_DIR}/debug_values_t37.exe")
set(pdb "${DEBUG_T37_OUTPUT_DIR}/debug_values_t37.pdb")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${DEBUG_T37_FIXTURE}"
        -g -O0 -o "${object}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
if(NOT compile_result EQUAL 0 OR NOT EXISTS "${object}")
    message(FATAL_ERROR
        "T37 O0 CodeView object compilation failed (${compile_result})\n"
        "stdout:\n${compile_output}\nstderr:\n${compile_error}")
endif()

execute_process(
    COMMAND "${LLVM_READOBJ}" --codeview "${object}"
    RESULT_VARIABLE codeview_result
    OUTPUT_VARIABLE codeview_dump
    ERROR_VARIABLE codeview_error)
foreach(required_record
        "FunctionType: int (int, int, int&)"
        "Type: int&"
        "VarName: produced"
        "Type: DebugValuesT37.SpecificError*"
        "VarName: specific"
        "DataOffset: __nsc_state_"
        "DisplayName: DebugValuesT37.GlobalCount"
        "DisplayName: DebugValuesT37.Widget.Created"
        "DisplayName: DebugValuesT37.Widget.Destroyed")
    string(FIND "${codeview_dump}" "${required_record}" record_index)
    if(record_index EQUAL -1)
        set(codeview_contract_valid FALSE)
    endif()
endforeach()
if(NOT codeview_result EQUAL 0 OR DEFINED codeview_contract_valid)
    message(FATAL_ERROR
        "T37 object is missing reference-shaped out, handler parameters, or typed globals\n"
        "${codeview_dump}\n${codeview_error}")
endif()

execute_process(
    COMMAND "${LLVM_OBJDUMP}" -d "${object}"
    RESULT_VARIABLE disassembly_result
    OUTPUT_VARIABLE disassembly
    ERROR_VARIABLE disassembly_error)
if(NOT disassembly_result EQUAL 0 OR
   NOT disassembly MATCHES "movl[ \t]+\\$0x2," OR
   NOT disassembly MATCHES "movl[ \t]+\\$0x1a,")
    message(FATAL_ERROR
        "T37 caller no longer materializes the tested value 2 and literal 26 at O0\n"
        "${disassembly}\n${disassembly_error}")
endif()

execute_process(
    COMMAND "${NSLINK_EXECUTABLE}"
        /nodefaultlib /subsystem:console
        "/entry:__nsc_method_DebugValuesT37$3A$3ADebugValuesT37$2EProgram__Main"
        /debug:full "/pdbaltpath:%_PDB%"
        "/out:${executable}" "/pdb:${pdb}"
        "/nsimports:${NSIMPORTS_FILE}" "${object}" "${RUNTIME_LIBRARY}"
    RESULT_VARIABLE link_result
    OUTPUT_VARIABLE link_output
    ERROR_VARIABLE link_error)
if(NOT link_result EQUAL 0 OR NOT EXISTS "${executable}" OR NOT EXISTS "${pdb}")
    message(FATAL_ERROR
        "T37 link against the generated A: runtime failed (${link_result})\n"
        "stdout:\n${link_output}\nstderr:\n${link_error}")
endif()

execute_process(
    COMMAND "${executable}"
    RESULT_VARIABLE execution_result
    OUTPUT_VARIABLE execution_output
    ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 42)
    message(FATAL_ERROR
        "T37 fixture changed behavior (${execution_result}, expected 42)\n"
        "stdout:\n${execution_output}\nstderr:\n${execution_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -summary -symbols "${pdb}"
    RESULT_VARIABLE symbols_result
    OUTPUT_VARIABLE symbols_dump
    ERROR_VARIABLE symbols_error)
foreach(parameter literal fromCaller produced specific)
    if(NOT symbols_dump MATCHES "S_LOCAL[^\n]*`${parameter}`[\n\r ]+type=[^\n]*[\n\r ]+[^\n]*flags = param")
        message(FATAL_ERROR
            "T37 PDB does not expose ${parameter} as a live argument\n"
            "${symbols_dump}\n${symbols_error}")
    endif()
endforeach()
if(NOT symbols_result EQUAL 0 OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`produced`[\n\r ]+type=[^\n]*\\(int&\\)" OR
   NOT symbols_dump MATCHES "S_BLOCK32[^\n]*``[\n\r ]+[^\n]*parent[^\n]*[\n\r ]+[^\n]*code size[^\n]*[\n\r ]+[^\n]*S_LOCAL[^\n]*`specific`")
    message(FATAL_ERROR
        "T37 PDB lost out indirection or the catch-handler argument scope\n"
        "${symbols_dump}\n${symbols_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -globals "${pdb}"
    RESULT_VARIABLE globals_result
    OUTPUT_VARIABLE globals_dump
    ERROR_VARIABLE globals_error)
foreach(global DebugValuesT37.GlobalCount DebugValuesT37.Widget.Created DebugValuesT37.Widget.Destroyed)
    string(REPLACE "." "\\." escaped_global "${global}")
    if(NOT globals_dump MATCHES "S_GDATA32[^\n]*`${escaped_global}`[\n\r ]+[^\n]*type = 0x[0-9A-Fa-f]+ \\(int\\)")
        message(FATAL_ERROR
            "T37 PDB global catalogue is missing typed ${global}\n"
            "${globals_dump}\n${globals_error}")
    endif()
endforeach()
if(NOT globals_result EQUAL 0)
    message(FATAL_ERROR "T37 PDB global inspection failed\n${globals_error}")
endif()

message(STATUS
    "NativeSharp T37 O0 arguments, out values, catch parameters and globals verified")
