if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED LLVM_PDBUTIL OR NOT EXISTS "${LLVM_PDBUTIL}")
    message(FATAL_ERROR "LLVM_PDBUTIL must name llvm-pdbutil")
endif()
if(NOT DEFINED LLVM_READOBJ OR NOT EXISTS "${LLVM_READOBJ}")
    message(FATAL_ERROR "LLVM_READOBJ must name llvm-readobj")
endif()
if(NOT DEFINED DEBUG_EXTREME_SOURCE_DIR OR
   NOT EXISTS "${DEBUG_EXTREME_SOURCE_DIR}/Producer.ns" OR
   NOT EXISTS "${DEBUG_EXTREME_SOURCE_DIR}/Consumer.ns")
    message(FATAL_ERROR "DEBUG_EXTREME_SOURCE_DIR must contain the extreme debug fixture")
endif()
if(NOT DEFINED DEBUG_OUTPUT_DIR)
    message(FATAL_ERROR "DEBUG_OUTPUT_DIR is required")
endif()
if(NOT DEFINED DEBUG_CORE_PROJECT OR NOT EXISTS "${DEBUG_CORE_PROJECT}")
    message(FATAL_ERROR "DEBUG_CORE_PROJECT must name NativeSharp.Core.nsproj")
endif()

file(REMOVE_RECURSE "${DEBUG_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEBUG_OUTPUT_DIR}/Core" "${DEBUG_OUTPUT_DIR}/Producer" "${DEBUG_OUTPUT_DIR}/Consumer")
get_filename_component(debug_core_source_dir "${DEBUG_CORE_PROJECT}" DIRECTORY)
file(COPY "${debug_core_source_dir}/" DESTINATION "${DEBUG_OUTPUT_DIR}/Core/Source")
file(COPY "${DEBUG_EXTREME_SOURCE_DIR}/Producer.ns" DESTINATION "${DEBUG_OUTPUT_DIR}/Producer")
file(COPY "${DEBUG_EXTREME_SOURCE_DIR}/Consumer.ns" DESTINATION "${DEBUG_OUTPUT_DIR}/Consumer")

set(producer_library "${DEBUG_OUTPUT_DIR}/Producer/DebugExtremeProducer.lib")
set(producer_metadata "${DEBUG_OUTPUT_DIR}/Producer/DebugExtremeProducer.nsmeta")
set(producer_project "${DEBUG_OUTPUT_DIR}/Producer/DebugExtremeProducer.nsproj")
set(core_library "${DEBUG_OUTPUT_DIR}/Core/NativeSharpCore.lib")
set(core_metadata "${DEBUG_OUTPUT_DIR}/Core/NativeSharpCore.nsmeta")
set(core_project "${DEBUG_OUTPUT_DIR}/Core/NativeSharpCore.nsproj")
set(consumer_project "${DEBUG_OUTPUT_DIR}/Consumer/DebugExtremeConsumer.nsproj")
set(consumer_executable "${DEBUG_OUTPUT_DIR}/Consumer/DebugExtremeConsumer.exe")
set(consumer_pdb "${DEBUG_OUTPUT_DIR}/Consumer/DebugExtremeConsumer.pdb")
file(TO_CMAKE_PATH "${producer_library}" producer_library_xml)
file(TO_CMAKE_PATH "${producer_metadata}" producer_metadata_xml)
file(TO_CMAKE_PATH "${core_library}" core_library_xml)
file(TO_CMAKE_PATH "${core_metadata}" core_metadata_xml)

file(WRITE "${core_project}"
    "<Project><PropertyGroup><Name>NativeSharpCore</Name>"
    "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
    "<ItemGroup><Compile Include=\"Source/**/*.ns\" /></ItemGroup></Project>")

file(WRITE "${producer_project}"
    "<Project><PropertyGroup><Name>DebugExtremeProducer</Name>"
    "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
    "<ItemGroup><Compile Include=\"Producer.ns\" />"
    "<LibraryReference Include=\"${core_library_xml}\" Metadata=\"${core_metadata_xml}\" />"
    "</ItemGroup></Project>")
file(WRITE "${consumer_project}"
    "<Project><PropertyGroup><Name>DebugExtremeConsumer</Name>"
    "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
    "<Compile Include=\"Consumer.ns\" />"
    # The producer's public metadata mentions the real Core SIMD types.  Keep the same precompiled
    # Core module present while loading that LibraryReference so its external type closure is
    # complete in the consumer too.
    "<LibraryReference Include=\"${core_library_xml}\" Metadata=\"${core_metadata_xml}\" />"
    "<LibraryReference Include=\"${producer_library_xml}\" "
    "Metadata=\"${producer_metadata_xml}\" />"
    "</ItemGroup></Project>")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib "${core_project}"
        -g -O0 "-fdebug-prefix-map=${DEBUG_OUTPUT_DIR}=/_/nativesharp/debug-extreme"
        -o "${core_library}"
    RESULT_VARIABLE core_result
    OUTPUT_VARIABLE core_output
    ERROR_VARIABLE core_error)
if(NOT core_result EQUAL 0 OR
   NOT EXISTS "${core_library}" OR NOT EXISTS "${core_metadata}")
    message(FATAL_ERROR
        "extreme debug Core dependency failed (${core_result})\n"
        "stdout:\n${core_output}\nstderr:\n${core_error}")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib "${producer_project}"
        -g -O0 "-fdebug-prefix-map=${DEBUG_OUTPUT_DIR}=/_/nativesharp/debug-extreme"
        -o "${producer_library}"
    RESULT_VARIABLE producer_result
    OUTPUT_VARIABLE producer_output
    ERROR_VARIABLE producer_error)
if(NOT producer_result EQUAL 0 OR
   NOT EXISTS "${producer_library}" OR NOT EXISTS "${producer_metadata}")
    message(FATAL_ERROR
        "extreme debug producer failed (${producer_result})\n"
        "stdout:\n${producer_output}\nstderr:\n${producer_error}")
endif()

execute_process(
    COMMAND "${LLVM_READOBJ}" --sections --codeview "${producer_library}"
    RESULT_VARIABLE archive_result
    OUTPUT_VARIABLE archive_dump
    ERROR_VARIABLE archive_error)
if(NOT archive_result EQUAL 0 OR
   NOT archive_dump MATCHES "Name: \\.debug\\\$S" OR
   NOT archive_dump MATCHES "Name: \\.debug\\\$T" OR
   NOT archive_dump MATCHES "DebugExtreme\\.ProducerApi\\.Invoke" OR
   NOT archive_dump MATCHES "FileChecksum")
    message(FATAL_ERROR
        "static producer did not retain complete CodeView archive members\n"
        "${archive_dump}\n${archive_error}")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${consumer_project}"
        -g -O0 "-fdebug-prefix-map=${DEBUG_OUTPUT_DIR}=/_/nativesharp/debug-extreme"
        -o "${consumer_executable}"
    RESULT_VARIABLE consumer_result
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error)
if(NOT consumer_result EQUAL 0 OR
   NOT EXISTS "${consumer_executable}" OR NOT EXISTS "${consumer_pdb}")
    message(FATAL_ERROR
        "extreme debug consumer failed (${consumer_result})\n"
        "stdout:\n${consumer_output}\nstderr:\n${consumer_error}")
endif()

execute_process(
    COMMAND "${consumer_executable}"
    RESULT_VARIABLE execution_result
    OUTPUT_VARIABLE execution_output
    ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 42)
    message(FATAL_ERROR
        "extreme debug consumer returned ${execution_result}, expected 42\n"
        "stdout:\n${execution_output}\nstderr:\n${execution_error}")
endif()

execute_process(
    COMMAND "${LLVM_READOBJ}" --coff-debug-directory "${consumer_executable}"
    RESULT_VARIABLE rsds_result
    OUTPUT_VARIABLE rsds_dump
    ERROR_VARIABLE rsds_error)
if(NOT rsds_result EQUAL 0 OR
   NOT rsds_dump MATCHES "Type: CodeView" OR
   NOT rsds_dump MATCHES "PDBFileName: DebugExtremeConsumer\\.pdb")
    message(FATAL_ERROR "consumer PE has no portable matching RSDS record\n${rsds_dump}\n${rsds_error}")
endif()

execute_process(
    COMMAND "${LLVM_PDBUTIL}" dump -summary -modules -files -types -symbols "${consumer_pdb}"
    RESULT_VARIABLE pdb_result
    OUTPUT_VARIABLE pdb_dump
    ERROR_VARIABLE pdb_error)
if(NOT pdb_result EQUAL 0 OR
   NOT pdb_dump MATCHES "Has Debug Info: true" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.ProducerApi\\.Invoke" OR
   NOT pdb_dump MATCHES "DebugExtremeConsumer\\.Program\\.Main" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.Counter\\.Read" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.Pair\\.Sum" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.Box<int>" OR
   NOT pdb_dump MATCHES "borrowed DebugExtreme\\.ICounter" OR
   NOT pdb_dump MATCHES "raii DebugExtreme\\.Counter" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.ProducerApi\\.Consume" OR
   NOT pdb_dump MATCHES "DebugExtreme\\.Counter\\.~Counter" OR
   NOT pdb_dump MATCHES "destruction-entry \\[generated\\]" OR
   NOT pdb_dump MATCHES "NativeSharp\\.Core\\.Numerics\\.Float32x4" OR
   NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Lanes`" OR
   NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`C3`" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`simdVector`" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`simdMatrix`" OR
   NOT pdb_dump MATCHES "DebugExtreme\.Transform\.delegate-cell" OR
   NOT pdb_dump MATCHES "LF_MEMBER[^\n]*`Code`[^\n]*Type = 0x1" OR
   NOT pdb_dump MATCHES "NativeSharp\.Win32\.HANDLE" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`managedDelegate`" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`functionPointer`[\n\r ]+type=[^\n]*int \(int\)\*" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`nativeHandle`" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`consumed`" OR
   NOT pdb_dump MATCHES "(S_LOCAL|S_CONSTANT)[^\n]*`seed`" OR
   NOT pdb_dump MATCHES "S_LOCAL[^\n]*`interfaceView`" OR
   NOT pdb_dump MATCHES "(S_LOCAL|S_CONSTANT)[^\n]*`answer`" OR
   NOT pdb_dump MATCHES "Producer\\.ns" OR
   NOT pdb_dump MATCHES "Consumer\\.ns" OR
   NOT pdb_dump MATCHES "S_DEFRANGE")
    message(FATAL_ERROR
        "final consumer PDB lost producer/consumer debug surface\n${pdb_dump}\n${pdb_error}")
endif()

message(STATUS "NativeSharp extreme O0 producer/consumer PDB corpus verified")
