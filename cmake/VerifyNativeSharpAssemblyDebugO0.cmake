if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED LLVM_PDBUTIL OR NOT EXISTS "${LLVM_PDBUTIL}")
    message(FATAL_ERROR "LLVM_PDBUTIL must name llvm-pdbutil")
endif()
if(NOT DEFINED ASSEMBLY_DEBUG_FIXTURE OR NOT EXISTS "${ASSEMBLY_DEBUG_FIXTURE}")
    message(FATAL_ERROR "ASSEMBLY_DEBUG_FIXTURE must name AssemblyDebug.nsproj")
endif()
if(NOT DEFINED ASSEMBLY_DEBUG_OUTPUT_DIR)
    message(FATAL_ERROR "ASSEMBLY_DEBUG_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${ASSEMBLY_DEBUG_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${ASSEMBLY_DEBUG_OUTPUT_DIR}")
get_filename_component(source_directory "${ASSEMBLY_DEBUG_FIXTURE}" DIRECTORY)
set(source "${source_directory}/Main.ns")
set(prefix_map "-fdebug-prefix-map=${source_directory}=/_/nativesharp/assembly-debug")
file(SHA256 "${source}" source_sha256)
string(TOUPPER "${source_sha256}" source_sha256)

set(executable "${ASSEMBLY_DEBUG_OUTPUT_DIR}/assembly_debug.exe")
set(pdb "${ASSEMBLY_DEBUG_OUTPUT_DIR}/assembly_debug.pdb")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${ASSEMBLY_DEBUG_FIXTURE}"
        -g "${prefix_map}" -O0 -o "${executable}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
if(NOT compile_result EQUAL 0 OR NOT EXISTS "${executable}" OR NOT EXISTS "${pdb}")
    message(FATAL_ERROR
        "O0 assembly-debug product compilation failed (${compile_result})\n"
        "stdout:\n${compile_output}\nstderr:\n${compile_error}")
endif()

execute_process(
    COMMAND "${executable}"
    RESULT_VARIABLE execution_result
    OUTPUT_VARIABLE execution_output
    ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 42)
    message(FATAL_ERROR
        "O0 assembly-debug product changed behavior (${execution_result}, expected 42)\n"
        "stdout:\n${execution_output}\nstderr:\n${execution_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -l "${pdb}"
    RESULT_VARIABLE lines_result
    OUTPUT_VARIABLE lines_dump
    ERROR_VARIABLE lines_error)
string(FIND "${lines_dump}" "${source}" physical_source_index)
if(NOT lines_result EQUAL 0 OR
   NOT lines_dump MATCHES "/_/nativesharp/assembly-debug/Main\\.ns \\(SHA-256:[ \n\r]+${source_sha256}\\)" OR
   NOT lines_dump MATCHES "[ \n]10 ([0-9A-F]+)" OR
   NOT lines_dump MATCHES "[ \n]11 ([0-9A-F]+)" OR
   NOT physical_source_index EQUAL -1)
    message(FATAL_ERROR
        "O0 PDB is missing remapped source identity or physical asm instruction lines\n"
        "${lines_dump}\n${lines_error}")
endif()
string(REGEX MATCH "[ \n]10 ([0-9A-F]+)" line10 "${lines_dump}")
set(line10_address "${CMAKE_MATCH_1}")
string(REGEX MATCH "[ \n]11 ([0-9A-F]+)" line11 "${lines_dump}")
set(line11_address "${CMAKE_MATCH_1}")
if(line10_address STREQUAL line11_address)
    message(FATAL_ERROR
        "consecutive asm instructions collapsed to one PDB stepping address\n${lines_dump}")
endif()

message(STATUS "NativeSharp O0 assembly PDB instruction stepping verified")
