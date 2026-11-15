# EN: Fixture for the integration scripts that consume binary SDK libraries: builds
#     NativeSharp.Core and NativeSharp.Core.Collections.Generic as static libraries with their
#     NSMETA into OUTPUT_DIR (NativeSharpCore.lib/.nsmeta and Generic.lib/.nsmeta), from copies of
#     the sources so the repository tree stays untouched.
# ES: Fixture de los scripts de integracion que consumen bibliotecas binarias del SDK: compila
#     NativeSharp.Core y NativeSharp.Core.Collections.Generic como bibliotecas estaticas con su
#     NSMETA en OUTPUT_DIR, a partir de copias de las fuentes para no tocar el arbol del repositorio.
if(NOT DEFINED COMPILER OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "COMPILER, SOURCE_ROOT and OUTPUT_DIR are required")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(COPY "${SOURCE_ROOT}/lib/NativeSharp.Core/" DESTINATION "${OUTPUT_DIR}/core"
    PATTERN "obj" EXCLUDE PATTERN "bin" EXCLUDE)
execute_process(
    COMMAND "${COMPILER}" -O0 --no-artifact-cache --emit-project-oop-lib
        "${OUTPUT_DIR}/core/NativeSharp.Core.nsproj" -o "${OUTPUT_DIR}/NativeSharpCore.lib"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE coreResult)
if(NOT coreResult EQUAL 0)
    message(FATAL_ERROR "NativeSharp.Core library failed with ${coreResult}")
endif()
file(COPY "${SOURCE_ROOT}/lib/NativeSharp.Core.Collections.Generic/" DESTINATION "${OUTPUT_DIR}/generic"
    PATTERN "obj" EXCLUDE PATTERN "bin" EXCLUDE)
set(genericProject "${OUTPUT_DIR}/generic/NativeSharp.Core.Collections.Generic.nsproj")
file(READ "${genericProject}" genericText)
string(REPLACE "<ProjectReference Include=\"../NativeSharp.Core/NativeSharp.Core.nsproj\" />"
    "<LibraryReference Include=\"../NativeSharpCore.lib\" Metadata=\"../NativeSharpCore.nsmeta\" />"
    genericText "${genericText}")
file(WRITE "${genericProject}" "${genericText}")
execute_process(
    COMMAND "${COMPILER}" -O0 --no-artifact-cache --emit-project-oop-lib "${genericProject}"
        -o "${OUTPUT_DIR}/Generic.lib"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE genericResult)
if(NOT genericResult EQUAL 0)
    message(FATAL_ERROR "NativeSharp.Core.Collections.Generic library failed with ${genericResult}")
endif()
