# Compile and run deterministic scalar boundary checks without artifact cache.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
if(DEFINED CORE_BINARY AND NOT CORE_BINARY STREQUAL "")
    if(NOT EXISTS "${CORE_BINARY}" OR NOT DEFINED CORE_METADATA OR NOT EXISTS "${CORE_METADATA}")
        message(FATAL_ERROR "The Core binary and its metadata must both exist")
    endif()
    get_filename_component(fixture_project "${PROJECT_FILE}" ABSOLUTE)
    get_filename_component(fixture_dir "${fixture_project}" DIRECTORY)
    get_filename_component(output_dir_absolute "${OUTPUT_DIR}" ABSOLUTE)
    file(RELATIVE_PATH fixture_source "${output_dir_absolute}" "${fixture_dir}/Program.ns")
    file(TO_CMAKE_PATH "${CORE_BINARY}" core_binary_path)
    file(TO_CMAKE_PATH "${CORE_METADATA}" core_metadata_path)
    set(PROJECT_FILE "${OUTPUT_DIR}/DeterministicMathBinary.nsproj")
    file(WRITE "${PROJECT_FILE}" "<Project><PropertyGroup><Name>DeterministicMathBinary</Name><OutputType>Exe</OutputType></PropertyGroup><ItemGroup><Compile Include=\"${fixture_source}\"/><LibraryReference Include=\"${core_binary_path}\" Metadata=\"${core_metadata_path}\"/></ItemGroup></Project>\n")
endif()
foreach(mode IN ITEMS precise deterministic)
foreach(level IN ITEMS O0 O1 O2 O3)
    set(program "${OUTPUT_DIR}/DeterministicMathBoundaries_${mode}_${level}.exe")
    execute_process(
        COMMAND cmake -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${PROJECT_FILE}"
            "-${level}" "--fp-mode=${mode}" --fp-denormal=preserve -o "${program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "${level} build failed: ${build_result}\n${build_output}\n${build_error}")
    endif()
    execute_process(COMMAND "${program}" TIMEOUT 10
        RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
    if(NOT "${run_result}" STREQUAL "0")
        message(FATAL_ERROR "${level} run failed: ${run_result}\n${run_output}\n${run_error}")
    endif()
endforeach()
endforeach()
