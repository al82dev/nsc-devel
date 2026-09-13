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
get_filename_component(debug_source_directory "${DEBUG_FIXTURE}" DIRECTORY)
set(debug_prefix_map "-fdebug-prefix-map=${debug_source_directory}=/_/nativesharp/debug-pdb")
file(SHA256 "${debug_source_directory}/Main.ns" main_source_sha256)
file(SHA256 "${debug_source_directory}/left/Shared.ns" left_source_sha256)
file(SHA256 "${debug_source_directory}/right/Shared.ns" right_source_sha256)
file(SHA256 "${debug_source_directory}/Types.ns" types_source_sha256)
string(TOUPPER "${main_source_sha256}" main_source_sha256)
string(TOUPPER "${left_source_sha256}" left_source_sha256)
string(TOUPPER "${right_source_sha256}" right_source_sha256)
string(TOUPPER "${types_source_sha256}" types_source_sha256)

set(line_object "${DEBUG_OUTPUT_DIR}/debug_lines.obj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${DEBUG_FIXTURE}"
        -gline-tables-only "${debug_prefix_map}" -O0 -o "${line_object}"
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
        -gline-tables-only "${debug_prefix_map}" -O0 -o "${line_executable}"
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
    COMMAND "${LLVM_PDBUTIL}" dump -summary -symbols "${line_pdb}"
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

if(NOT DEFINED DEBUG_OPTIMIZATION_LEVELS)
    set(DEBUG_OPTIMIZATION_LEVELS O0 O2)
endif()
if(NOT DEFINED DEBUG_REPRO_OPTIMIZATION_LEVEL)
    set(DEBUG_REPRO_OPTIMIZATION_LEVEL O2)
endif()
foreach(level IN LISTS DEBUG_OPTIMIZATION_LEVELS)
    string(TOLOWER "${level}" stem)
    set(object "${DEBUG_OUTPUT_DIR}/debug_${stem}.obj")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${DEBUG_FIXTURE}"
            -g0 -g "${debug_prefix_map}" "-${level}" -o "${object}"
        RESULT_VARIABLE object_compile_result
        OUTPUT_VARIABLE object_compile_output
        ERROR_VARIABLE object_compile_error)
    if(NOT object_compile_result EQUAL 0 OR NOT EXISTS "${object}")
        message(FATAL_ERROR
            "${level} full-debug object compilation failed (${object_compile_result})\n"
            "stdout:\n${object_compile_output}\nstderr:\n${object_compile_error}")
    endif()
    execute_process(
        COMMAND "${LLVM_READOBJ}" --sections --relocations --codeview "${object}"
        RESULT_VARIABLE object_inspection_result
        OUTPUT_VARIABLE object_dump
        ERROR_VARIABLE object_inspection_error)
    if(NOT object_inspection_result EQUAL 0 OR
       NOT object_dump MATCHES "Name: \\.debug\\\$S" OR
       NOT object_dump MATCHES "Name: \\.debug\\\$T" OR
       NOT object_dump MATCHES "Section \\([0-9]+\\) \\.debug\\\$S" OR
       NOT object_dump MATCHES "IMAGE_REL_AMD64_SECREL" OR
       NOT object_dump MATCHES "IMAGE_REL_AMD64_SECTION" OR
       NOT object_dump MATCHES "Compile3Sym" OR
       NOT object_dump MATCHES "GlobalProcIdSym" OR
       NOT object_dump MATCHES "FileChecksum" OR
       NOT object_dump MATCHES "DefRange")
        message(FATAL_ERROR
            "${level} COFF object is not a self-contained CodeView input for a PDB linker\n"
            "${object_inspection_error}")
    endif()

    set(executable "${DEBUG_OUTPUT_DIR}/debug_${stem}.exe")
    set(pdb "${DEBUG_OUTPUT_DIR}/debug_${stem}.pdb")
    execute_process(
        COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${DEBUG_FIXTURE}"
            -g0 -g "${debug_prefix_map}" "-${level}" -o "${executable}"
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
        COMMAND "${LLVM_PDBUTIL}" dump -summary -modules -files -types -symbols "${pdb}"
        RESULT_VARIABLE pdb_result
        OUTPUT_VARIABLE pdb_dump
        ERROR_VARIABLE pdb_error)
    if(NOT pdb_result EQUAL 0 OR
       NOT pdb_dump MATCHES "Has Debug Info: true" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Pair\\.Total" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.DispatchSource\\.ReadValue`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.HandlerHost\\.OnChanged`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.HandlerHost\\.Read`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Program\\.OnStaticChanged`" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Compute" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Program\\.Sum" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Program\\.MakeWide`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Program\\.SumWide`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Program\\.ExerciseEh`" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.UnwindResource\\.~UnwindResource`" OR
       NOT pdb_dump MATCHES "S_THUNK32[^`]*`[^`]*Increment[^`]*__c_boundary`" OR
       NOT pdb_dump MATCHES "S_THUNK32[^\n]*`ExportedIdentity`" OR
       NOT pdb_dump MATCHES "((S_GPROC32|S_LPROC32)[^\n]*`DebugFixture\\.GeneratedState\\.cctor \\[generated\\]`|S_INLINEES[^\n]*[\n\r ]+inlinee:[^\n]*DebugFixture\\.GeneratedState\\.ccto)" OR
       NOT pdb_dump MATCHES "S_GPROC32[^\n]*`DebugFixture\\.Resource\\.destruction-entry \\[generated\\]`" OR
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
       NOT pdb_dump MATCHES "LF_STRUCTURE[^\n]*DebugFixture\\.WideValue" OR
       NOT pdb_dump MATCHES "LF_ENUM[^\n]*DebugFixture\\.DebugMode" OR
       NOT pdb_dump MATCHES "LF_ENUMERATE[^\n]*Answer = 42" OR
       NOT pdb_dump MATCHES "LF_CLASS[^\n]*DebugFixture\\.DerivedRecord" OR
       NOT pdb_dump MATCHES "LF_BCLASS" OR
       NOT pdb_dump MATCHES "LF_UNION[^\n]*DebugFixture\\.NumberBits" OR
       NOT pdb_dump MATCHES "LF_STRUCTURE[^\n]*`string`" OR
       NOT pdb_dump MATCHES "LF_STRUCTURE[^\n]*`builtin:int\\[\\]`" OR
       NOT pdb_dump MATCHES "LF_STRUCTURE[^\n]*DebugFixture\\.Reader\\.delegate-cell" OR
       NOT pdb_dump MATCHES "DebugFixture\\.Box<int>" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Left`[^\n]*offset = 0[^\n]*attrs = public" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Right`[^\n]*offset = 4[^\n]*attrs = public" OR
       NOT pdb_dump MATCHES "sizeof 8" OR
       NOT pdb_dump MATCHES "sizeof 12" OR
       NOT pdb_dump MATCHES "int \\(DebugFixture\\.Pair\\)" OR
       NOT pdb_dump MATCHES "DebugFixture\\.WideValue \\(int, int" OR
       NOT pdb_dump MATCHES "int \\(DebugFixture\\.WideValue\\)" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`BaseValue`[^\n]*offset = 8" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`DerivedValue`[^\n]*offset = 16" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Whole`[^\n]*offset = 0" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Real`[^\n]*offset = 0" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Length`[^\n]*offset = 0" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Data`[^\n]*offset = 8" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Code`[^\n]*offset = 0" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Receiver`[^\n]*offset = 8" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`ContextDestroy`[^\n]*offset = 16" OR
       NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`CellFree`[^\n]*offset = 24" OR
       NOT pdb_dump MATCHES "S_DEFRANGE")
        message(FATAL_ERROR
            "${level} PDB is missing debug summary, functions, variables, types or ranges\n"
            "${pdb_dump}\n${pdb_error}")
    endif()
    # The catch-filter helper is a concrete callable at O0 and must remain visible there. At O2
    # LLVM is allowed to fold the filter into the funclet and omit the local procedure record; the
    # surrounding EH function, unwind tables, source lines, blocks, and ranges remain mandatory.
    if(level STREQUAL "O0" AND
       NOT pdb_dump MATCHES "S_LPROC32[^\n]*`DebugFixture\\.DebugProblem\\.catch-filter \\[generated\\]`")
        message(FATAL_ERROR
            "O0 PDB is missing the generated catch-filter procedure\n"
            "${pdb_dump}\n${pdb_error}")
    endif()
    if(pdb_dump MATCHES "S_THUNK32[^\n]*`DebugFixture\\.(DispatchSource\\.ReadValue|HandlerHost\\.(OnChanged|Read)|Program\\.OnStaticChanged)`")
        message(FATAL_ERROR
            "${level} PDB misclassified logical interface/virtual/handler methods as ABI thunks\n"
            "${pdb_dump}\n${pdb_error}")
    endif()

    execute_process(
        COMMAND "${LLVM_READOBJ}" --sections --unwind "${executable}"
        RESULT_VARIABLE unwind_result
        OUTPUT_VARIABLE unwind_dump
        ERROR_VARIABLE unwind_error)
    if(NOT unwind_result EQUAL 0 OR
       NOT unwind_dump MATCHES "Name: \\.pdata" OR
       NOT unwind_dump MATCHES "RuntimeFunction" OR
       NOT unwind_dump MATCHES "UnwindInfoAddress" OR
       NOT unwind_dump MATCHES "ExceptionHandler" OR
       NOT unwind_dump MATCHES "TerminateHandler" OR
       NOT unwind_dump MATCHES "Handler:")
        message(FATAL_ERROR
            "${level} PE is missing physical Win64 unwind coverage for dispatch or SEH frames\n"
            "${unwind_dump}\n${unwind_error}")
    endif()
    if(level STREQUAL "O0" AND
       (NOT pdb_dump MATCHES "S_BLOCK32[^\n]*``[\n\r ]+parent = [0-9]+, end = [0-9]+[\n\r ]+code size = [0-9]+, addr = [^\n]+[\n\r ]+[^\n]*S_LOCAL[^\n]*`scoped`"))
        message(FATAL_ERROR
            "O0 PDB is missing the nested lexical scope and its local\n"
            "${pdb_dump}\n${pdb_error}")
    endif()

    execute_process(
        COMMAND "${LLVM_PDBUTIL}" dump -l "${pdb}"
        RESULT_VARIABLE lines_result
        OUTPUT_VARIABLE lines_dump
        ERROR_VARIABLE lines_error)
    set(lines_valid TRUE)
    if(NOT lines_result EQUAL 0 OR
       NOT lines_dump MATCHES "/_/nativesharp/debug-pdb/Main\\.ns \\(SHA-256:[ \n\r]+${main_source_sha256}\\)" OR
       NOT lines_dump MATCHES "/_/nativesharp/debug-pdb/left/Shared\\.ns \\(SHA-256:[ \n\r]+${left_source_sha256}\\)" OR
       NOT lines_dump MATCHES "/_/nativesharp/debug-pdb/right/Shared\\.ns \\(SHA-256:[ \n\r]+${right_source_sha256}\\)" OR
       NOT lines_dump MATCHES "/_/nativesharp/debug-pdb/Types\\.ns \\(SHA-256:[ \n\r]+${types_source_sha256}\\)" OR
       lines_dump MATCHES "NSC-DEVEL-BRANCHES" OR
       NOT lines_dump MATCHES "[ \n]109 [0-9A-F]+" OR
       NOT lines_dump MATCHES "[ \n]227 [0-9A-F]+")
        set(lines_valid FALSE)
    endif()
    if(NOT lines_valid)
        message(FATAL_ERROR
            "${level} PDB is missing remapped multi-file SHA-256 source identity or expected lines\n"
            "${lines_dump}\n${lines_error}")
    endif()
    if(level STREQUAL "O0" AND
       (NOT lines_dump MATCHES "[ \n]209 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]210 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]219 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]220 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]41 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]163 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]167 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]171 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]176 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]180 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]182 [0-9A-F]+"))
        message(FATAL_ERROR
            "O0 PDB is missing dispatch, aggregate ABI, throw/catch/finally or unwind-destruction lines\n"
            "${lines_dump}\n${lines_error}")
    endif()
    if(level STREQUAL "O0" AND
       (NOT pdb_dump MATCHES "S_LOCAL[^\n]*`label`[\n\r ]+type=[^\n]*string" OR
        NOT pdb_dump MATCHES "(S_LOCAL|S_CONSTANT)[^\n]*`mode`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`values`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`derived`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`box`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`bits`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`typedReader`" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`capturedTypeValue`" OR
        NOT lines_dump MATCHES "[ \n]34 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]40 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]46 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]50 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]52 [0-9A-F]+" OR
        NOT lines_dump MATCHES "[ \n]58 [0-9A-F]+"))
        message(FATAL_ERROR
            "O0 PDB is missing representative NativeSharp types, captures or type-surface lines\n"
            "${pdb_dump}\n${pdb_error}\n${lines_dump}\n${lines_error}")
    endif()
    if(level STREQUAL "O2")
        string(REGEX MATCHALL "S_DEFRANGE_SUBFIELD_REGISTER" o2_fragment_ranges "${pdb_dump}")
        list(LENGTH o2_fragment_ranges o2_fragment_range_count)
    endif()
    if(level STREQUAL "O2" AND
       (NOT pdb_dump MATCHES "S_CONSTANT[^\n]*`answer`[\n\r ]+type[^\n]*value = 42" OR
        NOT pdb_dump MATCHES "S_CONSTANT[^\n]*`mode`[\n\r ]+type[^\n]*value = 42" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`label`[\n\r ]+type[^\n]*flags = optimized away" OR
        NOT pdb_dump MATCHES "S_LOCAL[^\n]*`capturedTypeValue`[\n\r ]+type[^\n]*flags = none[\n\r ]+[^\n]*S_DEFRANGE_REGISTER" OR
        pdb_dump MATCHES "S_LOCAL[^\n]*`capturedTypeValue`[\n\r ]+type[^\n]*flags = optimized away" OR
        NOT pdb_dump MATCHES "S_DEFRANGE_REGISTER[^\n]*[\n\r ]+register[^\n]*[\n\r ]+gaps = ..[0-9]+,[0-9]+.." OR
        NOT pdb_dump MATCHES "S_DEFRANGE_REGISTER_REL" OR
        o2_fragment_range_count LESS 2 OR
        NOT pdb_dump MATCHES "S_INLINESITE[^\n]*[\n\r ]+inlinee[^\n]*DebugFixture\.Program\.MaybeThrow" OR
        NOT pdb_dump MATCHES "S_INLINESITE[^\n]*[\n\r ]+inlinee[^\n]*DebugFixture\.Program\.Main\.lambda"))
        message(FATAL_ERROR
            "O2 PDB does not honestly cover constants, optimized-away values, register/stack/fragmented ranges, gaps or inline frames\n"
            "${pdb_dump}\n${pdb_error}")
    endif()
endforeach()

# Equal source trees copied to different physical roots must produce the same COFF bytes once
# both roots are mapped to the same published identity. This compares the producer-controlled
# artifact directly; linker-owned PDB identity is validated semantically above.
set(repro_source_a "${DEBUG_OUTPUT_DIR}/source-a")
set(repro_source_b "${DEBUG_OUTPUT_DIR}/source-b")
file(MAKE_DIRECTORY "${repro_source_a}" "${repro_source_b}")
file(COPY "${debug_source_directory}/" DESTINATION "${repro_source_a}")
file(COPY "${debug_source_directory}/" DESTINATION "${repro_source_b}")
set(repro_object_a "${DEBUG_OUTPUT_DIR}/repro-a.obj")
set(repro_object_b "${DEBUG_OUTPUT_DIR}/repro-b.obj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${repro_source_a}/DebugPdb.nsproj"
        -g "-fdebug-prefix-map=${repro_source_a}=/_/nativesharp/repro"
        "-${DEBUG_REPRO_OPTIMIZATION_LEVEL}" -o "${repro_object_a}"
    RESULT_VARIABLE repro_a_result
    OUTPUT_VARIABLE repro_a_output
    ERROR_VARIABLE repro_a_error)
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${repro_source_b}/DebugPdb.nsproj"
        -g "-fdebug-prefix-map=${repro_source_b}=/_/nativesharp/repro"
        "-${DEBUG_REPRO_OPTIMIZATION_LEVEL}" -o "${repro_object_b}"
    RESULT_VARIABLE repro_b_result
    OUTPUT_VARIABLE repro_b_output
    ERROR_VARIABLE repro_b_error)
if(NOT repro_a_result EQUAL 0 OR NOT repro_b_result EQUAL 0)
    message(FATAL_ERROR
        "remapped reproducibility compilation failed\n"
        "A stdout:\n${repro_a_output}\nA stderr:\n${repro_a_error}\n"
        "B stdout:\n${repro_b_output}\nB stderr:\n${repro_b_error}")
endif()
file(SHA256 "${repro_object_a}" repro_sha256_a)
file(SHA256 "${repro_object_b}" repro_sha256_b)
if(NOT repro_sha256_a STREQUAL repro_sha256_b)
    message(FATAL_ERROR
        "equal sources under equal remapping produced different COFF objects\n"
        "A=${repro_sha256_a}\nB=${repro_sha256_b}")
endif()

# A source-byte change that leaves program semantics untouched must still change the debug
# artifact, allowing a consumer to reject a stale or same-basename source file.
file(APPEND "${repro_source_b}/right/Shared.ns" "\n// source identity mutation\n")
set(modified_object "${DEBUG_OUTPUT_DIR}/modified-source.obj")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-obj "${repro_source_b}/DebugPdb.nsproj"
        -g "-fdebug-prefix-map=${repro_source_b}=/_/nativesharp/repro"
        "-${DEBUG_REPRO_OPTIMIZATION_LEVEL}" -o "${modified_object}"
    RESULT_VARIABLE modified_result
    OUTPUT_VARIABLE modified_output
    ERROR_VARIABLE modified_error)
if(NOT modified_result EQUAL 0)
    message(FATAL_ERROR
        "modified-source identity compilation failed\n"
        "stdout:\n${modified_output}\nstderr:\n${modified_error}")
endif()
file(SHA256 "${modified_object}" modified_sha256)
if(modified_sha256 STREQUAL repro_sha256_a)
    message(FATAL_ERROR "a source-byte change did not change the CodeView object identity")
endif()

string(JOIN "," debug_profiles ${DEBUG_OPTIMIZATION_LEVELS})
message(STATUS
    "NativeSharp ${debug_profiles} CodeView/PDB source identity and remapping verified")
