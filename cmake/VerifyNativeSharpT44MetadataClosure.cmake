if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED FIXTURE_ROOT OR NOT EXISTS "${FIXTURE_ROOT}/Dependency.ns")
    message(FATAL_ERROR "FIXTURE_ROOT must name the T44 fixture")
endif()
if(NOT DEFINED RUNTIME_LIBRARY OR NOT EXISTS "${RUNTIME_LIBRARY}")
    message(FATAL_ERROR "RUNTIME_LIBRARY must name the generated NativeSharp runtime")
endif()
if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/Dependency" "${OUTPUT_DIR}/Package" "${OUTPUT_DIR}/Consumer")
set(ENV{NSC_WINDOWS_RUNTIME_LIBRARY} "${RUNTIME_LIBRARY}")
file(COPY "${FIXTURE_ROOT}/Dependency.ns" DESTINATION "${OUTPUT_DIR}/Dependency")
file(COPY "${FIXTURE_ROOT}/Package.ns" DESTINATION "${OUTPUT_DIR}/Package")
file(COPY "${FIXTURE_ROOT}/Consumer.ns" DESTINATION "${OUTPUT_DIR}/Consumer")

set(dependency_lib "${OUTPUT_DIR}/T44Dependency.lib")
set(dependency_meta "${OUTPUT_DIR}/T44Dependency.nsmeta")
set(package_lib "${OUTPUT_DIR}/T44Package.lib")
set(package_meta "${OUTPUT_DIR}/T44Package.nsmeta")
set(consumer_exe "${OUTPUT_DIR}/T44Consumer.exe")
file(TO_CMAKE_PATH "${OUTPUT_DIR}" output_xml)
file(TO_CMAKE_PATH "${consumer_exe}" consumer_exe_xml)

file(WRITE "${OUTPUT_DIR}/Dependency/T44Dependency.nsproj"
    "<Project><PropertyGroup><Name>T44Dependency</Name><Language>3</Language>"
    "<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>"
    "<Compile Include=\"Dependency.ns\" /></ItemGroup></Project>")
file(WRITE "${OUTPUT_DIR}/Package/T44Package.nsproj"
    "<Project><PropertyGroup><Name>T44Package</Name><Language>3</Language>"
    "<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>"
    "<Compile Include=\"Package.ns\" />"
    "<LibraryReference Include=\"T44Dependency.lib\" Metadata=\"T44Dependency.nsmeta\" />"
    "</ItemGroup></Project>")
file(WRITE "${OUTPUT_DIR}/Consumer/T44Consumer.nsproj"
    "<Project><PropertyGroup><Name>T44Consumer</Name><Language>3</Language>"
    "<OutputType>Exe</OutputType><OptimizationLevel>O0</OptimizationLevel>"
    "<OutputPath>${consumer_exe_xml}</OutputPath>"
    "<LibraryReferenceRoot>${output_xml}</LibraryReferenceRoot>"
    "</PropertyGroup><ItemGroup><Compile Include=\"Consumer.ns\" />"
    "<LibraryReference Include=\"T44Package.lib\" Metadata=\"T44Package.nsmeta\" />"
    "</ItemGroup></Project>")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib
        "${OUTPUT_DIR}/Dependency/T44Dependency.nsproj" -O0 -g0 -o "${dependency_lib}"
    RESULT_VARIABLE dependency_result OUTPUT_VARIABLE dependency_output ERROR_VARIABLE dependency_error)
if(NOT dependency_result EQUAL 0 OR NOT EXISTS "${dependency_lib}" OR NOT EXISTS "${dependency_meta}")
    message(FATAL_ERROR "T44 dependency failed (${dependency_result})\n${dependency_output}\n${dependency_error}")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib
        "${OUTPUT_DIR}/Package/T44Package.nsproj" -O0 -g0
        --library-reference-root "${OUTPUT_DIR}" -o "${package_lib}"
    RESULT_VARIABLE package_result OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT EXISTS "${package_lib}" OR NOT EXISTS "${package_meta}")
    message(FATAL_ERROR "T44 package failed (${package_result})\n${package_output}\n${package_error}")
endif()

# The consumer names only T44Package. NSMETA 1.25 must locate T44Dependency, link both archives,
# retain the package's private kernel32 import, and make the selected language visible.
execute_process(
    COMMAND "${NSC_EXECUTABLE}" build "${OUTPUT_DIR}/Consumer/T44Consumer.nsproj"
        --no-restore --linker nslink -O0 --library-reference-root "${OUTPUT_DIR}"
    RESULT_VARIABLE consumer_result OUTPUT_VARIABLE consumer_output ERROR_VARIABLE consumer_error)
if(NOT consumer_result EQUAL 0 OR NOT EXISTS "${consumer_exe}" OR
   NOT consumer_output MATCHES "Build succeeded \\(Language 3\\)")
    message(FATAL_ERROR "T44 consumer failed (${consumer_result})\n${consumer_output}\n${consumer_error}")
endif()
execute_process(
    COMMAND "${consumer_exe}"
    RESULT_VARIABLE execution_result OUTPUT_VARIABLE execution_output ERROR_VARIABLE execution_error)
if(NOT execution_result EQUAL 0)
    message(FATAL_ERROR "T44 executable returned ${execution_result}\n${execution_output}\n${execution_error}")
endif()

# Replace the certified dependency metadata with a second valid module contract. The consumer must
# reject the mismatch from NSMETA before entering native link.
file(RENAME "${dependency_meta}" "${OUTPUT_DIR}/T44Dependency.original.nsmeta")
file(WRITE "${OUTPUT_DIR}/Dependency/Dependency.ns"
    "namespace T44.Dependency { public class ValueProvider { public static long Forty() { return 40; } } }")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-lib
        "${OUTPUT_DIR}/Dependency/T44Dependency.nsproj" -O0 -g0 -o "${dependency_lib}"
    RESULT_VARIABLE replacement_result OUTPUT_VARIABLE replacement_output ERROR_VARIABLE replacement_error)
if(NOT replacement_result EQUAL 0)
    message(FATAL_ERROR "T44 replacement dependency failed\n${replacement_output}\n${replacement_error}")
endif()
execute_process(
    COMMAND "${NSC_EXECUTABLE}" build "${OUTPUT_DIR}/Consumer/T44Consumer.nsproj"
        --no-restore --linker nslink -O0 --library-reference-root "${OUTPUT_DIR}"
    RESULT_VARIABLE mismatch_result OUTPUT_VARIABLE mismatch_output ERROR_VARIABLE mismatch_error)
if(mismatch_result EQUAL 0 OR NOT mismatch_error MATCHES "ABI fingerprint mismatch")
    message(FATAL_ERROR "T44 incompatible closure was not rejected causally\n${mismatch_output}\n${mismatch_error}")
endif()

message(STATUS "NativeSharp T44 metadata closure checks passed")
