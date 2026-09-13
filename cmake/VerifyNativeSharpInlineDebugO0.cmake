if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED LLVM_PDBUTIL OR NOT EXISTS "${LLVM_PDBUTIL}")
    message(FATAL_ERROR "LLVM_PDBUTIL must name llvm-pdbutil")
endif()
if(NOT DEFINED INLINE_DEBUG_FIXTURE OR NOT EXISTS "${INLINE_DEBUG_FIXTURE}")
    message(FATAL_ERROR "INLINE_DEBUG_FIXTURE must name InlineDebug.nsproj")
endif()
if(NOT DEFINED INLINE_DEBUG_OUTPUT_DIR)
    message(FATAL_ERROR "INLINE_DEBUG_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${INLINE_DEBUG_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${INLINE_DEBUG_OUTPUT_DIR}")
get_filename_component(inline_source_directory "${INLINE_DEBUG_FIXTURE}" DIRECTORY)
set(inline_source "${inline_source_directory}/Main.ns")
set(prefix_map "-fdebug-prefix-map=${inline_source_directory}=/_/nativesharp/inline-debug")
file(SHA256 "${inline_source}" source_sha256)
string(TOUPPER "${source_sha256}" source_sha256)

set(executable "${INLINE_DEBUG_OUTPUT_DIR}/inline_debug.exe")
set(pdb "${INLINE_DEBUG_OUTPUT_DIR}/inline_debug.pdb")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${INLINE_DEBUG_FIXTURE}"
        -g "${prefix_map}" -O0 -o "${executable}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
if(NOT compile_result EQUAL 0 OR NOT EXISTS "${executable}" OR NOT EXISTS "${pdb}")
    message(FATAL_ERROR
        "O0 inline-debug product compilation failed (${compile_result})\n"
        "stdout:\n${compile_output}\nstderr:\n${compile_error}")
endif()

execute_process(
    COMMAND "${executable}"
    RESULT_VARIABLE execution_result
    OUTPUT_VARIABLE execution_output
    ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 42)
    message(FATAL_ERROR
        "O0 inline-debug product changed behavior (${execution_result}, expected 42)\n"
        "stdout:\n${execution_output}\nstderr:\n${execution_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -summary -symbols "${pdb}"
    RESULT_VARIABLE symbols_result
    OUTPUT_VARIABLE symbols_dump
    ERROR_VARIABLE symbols_error)
if(NOT symbols_result EQUAL 0 OR
   NOT symbols_dump MATCHES "Has Debug Info: true" OR
   NOT symbols_dump MATCHES "S_INLINESITE[^\n]*[\n\r ]+inlinee[^\n]*InlineDebug\\.Program\\.RequiredStep" OR
   NOT symbols_dump MATCHES "S_GPROC32[^\n]*`InlineDebug\\.Program\\.PreferredStep`" OR
   NOT symbols_dump MATCHES "S_GPROC32[^\n]*`InlineDebug\\.Program\\.BoundaryStep`" OR
   NOT symbols_dump MATCHES "S_GPROC32[^\n]*`InlineDebug\\.Program\\.Main`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`requiredLocal`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`preferredLocal`" OR
   NOT symbols_dump MATCHES "S_LOCAL[^\n]*`boundaryLocal`" OR
   symbols_dump MATCHES "S_INLINESITE[^\n]*[\n\r ]+inlinee[^\n]*InlineDebug\\.Program\\.(PreferredStep|BoundaryStep)")
    message(FATAL_ERROR
        "O0 PDB does not preserve the required inline frame and explicit call boundaries\n"
        "${symbols_dump}\n${symbols_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -l "${pdb}"
    RESULT_VARIABLE lines_result
    OUTPUT_VARIABLE lines_dump
    ERROR_VARIABLE lines_error)
if(NOT lines_result EQUAL 0 OR
   NOT lines_dump MATCHES "/_/nativesharp/inline-debug/Main\\.ns \\(SHA-256:[ \n\r]+${source_sha256}\\)" OR
   NOT lines_dump MATCHES "[ \n]7 [0-9A-F]+" OR
   NOT lines_dump MATCHES "[ \n]8 [0-9A-F]+" OR
   NOT lines_dump MATCHES "[ \n]25 [0-9A-F]+" OR
   NOT lines_dump MATCHES "[ \n]26 [0-9A-F]+" OR
   NOT lines_dump MATCHES "[ \n]27 [0-9A-F]+" OR
   lines_dump MATCHES "nsc-devel")
    message(FATAL_ERROR
        "O0 PDB does not retain remapped source identity, inline-body lines, and call-site lines\n"
        "${lines_dump}\n${lines_error}")
endif()

message(STATUS "NativeSharp O0 explicit inline stepping verified")
