file(MAKE_DIRECTORY "${OUTPUT_DIR}")
configure_file("${SOURCE_ROOT}/selfhost/nsc/Types/TypeSystem.ns" "${OUTPUT_DIR}/TypeSystem.ns" COPYONLY)
configure_file("${SOURCE_ROOT}/tests/integration/selfhost_sbyte_type/Program.ns" "${OUTPUT_DIR}/Program.ns" COPYONLY)
file(WRITE "${OUTPUT_DIR}/Probe.nsproj" "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup><Compile Include=\"Program.ns\"/><Compile Include=\"TypeSystem.ns\"/><ProjectReference Include=\"${SOURCE_ROOT}/lib/NativeSharp.Core/NativeSharp.Core.nsproj\"/><ProjectReference Include=\"${SOURCE_ROOT}/lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj\"/></ItemGroup></Project>")
foreach(level IN ITEMS O0 O2)
    set(program "${OUTPUT_DIR}/Probe-${level}.exe")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}" "NSC_LINKER=${NSLINK_EXECUTABLE}"
        "${NSC_EXECUTABLE}" --emit-project-oop-exe "${OUTPUT_DIR}/Probe.nsproj"
        -${level} -g0 -o "${program}" --no-artifact-cache --no-error-log
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "SByte type build ${level} failed: ${build_result}\n${build_out}\n${build_err}")
    endif()
    execute_process(COMMAND "${program}" RESULT_VARIABLE run_result)
    if(NOT run_result EQUAL 0)
        message(FATAL_ERROR "SByte type execution ${level} failed: ${run_result}")
    endif()
endforeach()
