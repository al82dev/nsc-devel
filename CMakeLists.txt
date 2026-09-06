cmake_minimum_required(VERSION 3.25)

project(
    NativeSharp
    VERSION 0.8.0
    DESCRIPTION "NativeSharp stage 57 -- ownership, enum and cross-project correctness"
    LANGUAGES CXX)

option(NSC_BUILD_TESTS "Build NativeSharp frontend tests" ON)
option(NSC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# CC1-000 / version de build. La version compatible es estable; la identidad visible cambia en
# CADA construccion real de nsc, incluso en un build incremental que no vuelve a configurar CMake.
# `nsc_build_identity` es un target siempre desactualizado y nsc_driver depende de el. Solo
# Version.cpp incluye el header generado: asi no existe una carrera con los muchos tests que
# incluyen Version.h y tampoco se obliga a recompilar todo el frontend por cambiar un string.
set(NSC_COMPILER_RELEASE_VERSION "${PROJECT_VERSION}-alpha")
set(NSC_GENERATED_BASIC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/nativesharp/Basic")
set(NSC_GENERATED_BUILD_ID_HEADER "${NSC_GENERATED_BASIC_DIR}/BuildId.h")
set(NSC_BUILD_ID_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildIdentity.cmake")
set(NSC_BUILD_ID_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/include/nativesharp/Basic/BuildId.h.in")
file(MAKE_DIRECTORY "${NSC_GENERATED_BASIC_DIR}")

# La copia inicial permite que el primer analisis de dependencias encuentre el header. El target
# de build lo reemplaza justo antes de compilar nsc_driver y esa segunda identidad es la que queda
# dentro del ejecutable.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DNSC_OUTPUT=${NSC_GENERATED_BUILD_ID_HEADER}"
        "-DNSC_TEMPLATE=${NSC_BUILD_ID_TEMPLATE}"
        "-DNSC_VERSION_BASE=${NSC_COMPILER_RELEASE_VERSION}"
        -P "${NSC_BUILD_ID_GENERATOR}"
    RESULT_VARIABLE NSC_INITIAL_BUILD_ID_RESULT)
if(NOT NSC_INITIAL_BUILD_ID_RESULT EQUAL 0)
    message(FATAL_ERROR "No se pudo generar la identidad inicial de nsc")
endif()

add_custom_target(nsc_build_identity
    COMMAND "${CMAKE_COMMAND}"
        "-DNSC_OUTPUT=${NSC_GENERATED_BUILD_ID_HEADER}"
        "-DNSC_TEMPLATE=${NSC_BUILD_ID_TEMPLATE}"
        "-DNSC_VERSION_BASE=${NSC_COMPILER_RELEASE_VERSION}"
        -P "${NSC_BUILD_ID_GENERATOR}"
    BYPRODUCTS "${NSC_GENERATED_BUILD_ID_HEADER}"
    COMMENT "Generando version unica para esta construccion de nsc"
    VERBATIM)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")
message(STATUS "NativeSharp compiler release: ${NSC_COMPILER_RELEASE_VERSION}; build id generado al construir")

# Organizacion explicita de la salida del build, dentro de ${CMAKE_BINARY_DIR} (el directorio
# que le pasas a "cmake -B"), no en otro lado y no por accidente de donde CMake decida ponerla
# por defecto:
#
#   build/bin/    -- nsc (el compilador). Unico ejecutable "de producto" de este proyecto.
#   build/lib/    -- las 7 librerias estaticas internas (nsc_compilation, nsc_target,
#                    nsc_frontend, nsc_llvm, nsc_runtime, nsc_linker, nsc_driver) -- .a en
#                    Linux/macOS, .lib en Windows/MSVC. Estas variables tambien cubren
#                    CMAKE_LIBRARY_OUTPUT_DIRECTORY (.so/.dll si alguna vez se agrega una
#                    libreria compartida) e IMPORTED_IMPLIB en Windows.
#   build/tests/  -- los ~95 ejecutables de test (solo si NSC_BUILD_TESTS=ON, el default).
#
# CMAKE_ARCHIVE_OUTPUT_DIRECTORY / CMAKE_LIBRARY_OUTPUT_DIRECTORY se fijan UNA sola vez aqui,
# antes de cualquier add_library(), y aplican a las 7 sin excepcion -- no hay split como con los
# ejecutables porque aqui no hace falta separar "producto" de "test", todas son librerias
# internas del propio compilador.
#
# CMAKE_RUNTIME_OUTPUT_DIRECTORY (ejecutables) SI cambia de valor una segunda vez mas abajo,
# justo antes del primer add_executable() de test (ver el bloque "if(NSC_BUILD_TESTS)"), porque
# ahi si hace falta separar nsc de los tests. CMake lee el valor ACTUAL de cada variable
# "*_OUTPUT_DIRECTORY" en el momento de declarar cada add_executable()/add_library() -- por eso
# alcanza con reasignarlas por bloque, sin tocar ninguna de las decenas de llamadas a
# add_executable/add_library/nsc_add_unit_test que vienen despues.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(nsc_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(NSC_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow)
        if(NSC_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(nsc_compilation STATIC
    src/Compilation/SimpleXmlParser.cpp
    src/Compilation/GlobPattern.cpp
    src/Compilation/Project.cpp
    src/Compilation/ProjectSourceSet.cpp
    src/Compilation/ProjectLoader.cpp
    src/Compilation/ProjectDumper.cpp
    src/Compilation/ProjectGraph.cpp
    src/Compilation/ProjectGraphLoader.cpp
    src/Compilation/ProjectGraphDumper.cpp
    src/Compilation/TypeIdentity.cpp
    src/Compilation/PartialTypeAggregator.cpp)

target_include_directories(nsc_compilation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nsc_compilation PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_compilation)

add_library(nsc_target STATIC
    src/Target/TargetInfo.cpp
    src/Target/TargetDataLayout.cpp)

target_include_directories(nsc_target PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nsc_target PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_target)

add_library(nsc_frontend STATIC
    src/Source/SourceFile.cpp
    src/Source/SourceManager.cpp
    src/Diagnostics/Diagnostic.cpp
    src/Diagnostics/DiagnosticEngine.cpp
    src/Diagnostics/DiagnosticRenderer.cpp
    src/Lexer/Token.cpp
    src/Lexer/TokenKind.cpp
    src/Lexer/TokenDumper.cpp
    src/Lexer/Lexer.cpp
    src/AST/AstArena.cpp
    src/AST/AstNode.cpp
    src/AST/AstNodeKind.cpp
    src/AST/TypeSyntax.cpp
    src/AST/AstDumper.cpp
    src/Parser/Parser.cpp
    src/Sema/OwnershipKind.cpp
    src/Sema/TypeKind.cpp
    src/Sema/Type.cpp
    src/Sema/TypeContext.cpp
    src/Sema/SymbolKind.cpp
    src/Sema/Symbol.cpp
    src/Sema/Scope.cpp
    src/Sema/ConstantValue.cpp
    src/Sema/ConversionKind.cpp
    src/Sema/SemanticModel.cpp
    src/Sema/SymbolDumper.cpp
    src/Sema/SemanticAnalyzer.cpp
    src/Sema/SemanticDumper.cpp
    src/HIR/HirNodeKind.cpp
    src/HIR/HirNode.cpp
    src/HIR/HirArena.cpp
    src/HIR/HirBuilder.cpp
    src/HIR/HirDumper.cpp
    src/HIR/HirVerifier.cpp
    src/MIR/MirOpcode.cpp
    src/MIR/MirValue.cpp
    src/MIR/MirInstruction.cpp
    src/MIR/MirBlock.cpp
    src/MIR/MirGlobal.cpp
    src/MIR/MirFunction.cpp
    src/MIR/MirModule.cpp
    src/MIR/MirArena.cpp
    src/MIR/MirBuilder.cpp
    src/MIR/MirDumper.cpp
    src/MIR/MirVerifier.cpp
    src/CFG/CfgEdgeKind.cpp
    src/CFG/CfgEdge.cpp
    src/CFG/CfgNode.cpp
    src/CFG/CfgFunction.cpp
    src/CFG/CfgModule.cpp
    src/CFG/CfgArena.cpp
    src/CFG/CfgTraversal.cpp
    src/CFG/CfgBuilder.cpp
    src/CFG/CfgDumper.cpp
    src/CFG/CfgVerifier.cpp
    src/Dominance/DominatorTreeNode.cpp
    src/Dominance/DominatorTree.cpp
    src/Dominance/DominanceFrontier.cpp
    src/Dominance/DominanceFunction.cpp
    src/Dominance/DominanceModule.cpp
    src/Dominance/DominanceArena.cpp
    src/Dominance/DominanceBuilder.cpp
    src/Dominance/DominanceDumper.cpp
    src/Dominance/DominanceVerifier.cpp
    src/SSA/SsaFunction.cpp
    src/SSA/SsaModule.cpp
    src/SSA/SsaArena.cpp
    src/SSA/SsaPromotability.cpp
    src/SSA/SsaBuilder.cpp
    src/SSA/SsaDumper.cpp
    src/SSA/SsaVerifier.cpp
    src/Passes/PreservedAnalyses.cpp
    src/Passes/PassRequirements.cpp
    src/Passes/PassContext.cpp
    src/Passes/OptimizationModule.cpp
    src/Passes/OptimizationArena.cpp
    src/Passes/OptimizationBuilder.cpp
    src/Passes/AnalysisManager.cpp
    src/Passes/MirEffectClassifier.cpp
    src/Passes/MirRewriter.cpp
    src/Passes/ConstantFolder.cpp
    src/Passes/OptimizationVerifier.cpp
    src/Passes/PhiSimplificationPass.cpp
    src/Passes/CopyPropagationPass.cpp
    src/Passes/ConstantPropagationPass.cpp
    src/Passes/DeadCodeEliminationPass.cpp
    src/Passes/SimplifyControlFlowPass.cpp
    src/Passes/PassManager.cpp
    src/Passes/OptimizationDumper.cpp
    src/Passes/PassPipelineDumper.cpp
    src/Frontend/FrontendCompilation.cpp
    src/Frontend/ProjectSyntaxUnit.cpp
    src/Frontend/NamespaceSymbol.cpp
    src/Frontend/QualifiedNameLadder.cpp
    src/Frontend/ProjectGenericConstraintModel.cpp
    src/Frontend/ProjectGenericModel.cpp
    src/Frontend/ProjectGenericModelDumper.cpp
    src/Frontend/ProjectNameLookup.cpp
    src/Frontend/ProjectNameLookupDumper.cpp
    src/Frontend/ProjectMemberModel.cpp
    src/Frontend/ProjectMemberModelDumper.cpp
    src/Frontend/ProjectInheritanceModel.cpp
    src/Frontend/ProjectInheritanceModelDumper.cpp
    src/Frontend/ProjectInterfaceModel.cpp
    src/Frontend/ProjectInterfaceModelDumper.cpp
    src/Frontend/ProjectMemberAccessibility.cpp
    src/Frontend/ProjectVirtualDispatchModel.cpp
    src/Frontend/ProjectVirtualDispatchModelDumper.cpp
    src/Frontend/ProjectDestructionModel.cpp
    src/Frontend/ProjectDestructionModelDumper.cpp
    src/Frontend/ProjectTypeConversion.cpp
    src/Frontend/ProjectIntegerEvaluator.cpp
    src/Frontend/ProjectConversionClassifier.cpp
    src/Frontend/ProjectMethodBodyModel.cpp
    src/Frontend/ProjectMethodBodyModelDumper.cpp
    src/Frontend/ProjectConstructionModel.cpp
    src/Frontend/ProjectConstructionModelDumper.cpp
    src/Frontend/ProjectObjectLayoutModel.cpp
    src/Frontend/ProjectObjectLayoutModelDumper.cpp
    src/Frontend/ProjectOopHirModel.cpp
    src/Frontend/ProjectOopHirModelDumper.cpp
    src/Frontend/ProjectOopMirModel.cpp
    src/Frontend/ProjectOopMirModelDumper.cpp
    src/Frontend/ProjectCompilation.cpp
    src/Frontend/ProjectCompilationDumper.cpp)

target_include_directories(nsc_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_frontend PUBLIC nsc_target nsc_compilation)
target_compile_features(nsc_frontend PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_frontend)

# Delivery 18: LLVM is a real backend dependency. Prefer an installed LLVM CMake
# package, but allow a monolithic libLLVM fallback for environments that ship
# the runtime library without development headers. The implementation uses the
# stable LLVM C ABI so both paths execute the same lowering code.
find_package(LLVM CONFIG QUIET)
set(NSC_LLVM_LINK_LIBRARIES)
if(LLVM_FOUND)
    # Bug B-008 (audit "LLVM minimum version mismatch"): this used to declare LLVM 19 as the
    # minimum, which stopped being true the moment src/LLVM/LlvmOptimizer.cpp started calling
    # LLVMRunPassesOnFunction for the bug B-001 fix. That entry point does NOT exist in LLVM 19's
    # C API -- verified directly against the real llvm-19-dev package's
    # llvm-c/Transforms/PassBuilder.h, which declares only the module-level LLVMRunPasses; it first
    # appears in LLVM 20. With the old declaration, configuring against a genuine LLVM 19 passed
    # here and then failed later at compile time with an unresolved symbol, which is exactly the
    # kind of contract break a version gate exists to prevent.
    if(LLVM_PACKAGE_VERSION VERSION_LESS 20)
        message(FATAL_ERROR
            "NativeSharp requires LLVM 20 or newer (LLVMRunPassesOnFunction, used by the bug B-001 "
            "-O0 SEH legalization in src/LLVM/LlvmOptimizer.cpp, is not present in LLVM 19's C API); "
            "found LLVM ${LLVM_PACKAGE_VERSION}")
    endif()
    message(STATUS "NativeSharp LLVM backend: LLVM ${LLVM_PACKAGE_VERSION} via LLVMConfig.cmake")
    if(TARGET LLVM)
        list(APPEND NSC_LLVM_LINK_LIBRARIES LLVM)
    else()
        llvm_map_components_to_libnames(
            NSC_LLVM_COMPONENT_LIBS
            core
            analysis
            support
            target
            nativecodegen
            passes
            X86)
        list(APPEND NSC_LLVM_LINK_LIBRARIES ${NSC_LLVM_COMPONENT_LIBS})
    endif()
else()
    find_library(
        NSC_LLVM_MONOLITHIC_LIBRARY
        # Bug B-008: LLVM-19 deliberately dropped from this list -- see the version gate above for
        # why 20 is now the real minimum. This fallback path has no LLVM_PACKAGE_VERSION to test,
        # so not naming the too-old soname is the only gate available here.
        NAMES LLVM LLVM-24 LLVM-23 LLVM-22 LLVM-21 LLVM-20
        REQUIRED)
    message(STATUS "NativeSharp LLVM backend: monolithic runtime ${NSC_LLVM_MONOLITHIC_LIBRARY}")
    list(APPEND NSC_LLVM_LINK_LIBRARIES ${NSC_LLVM_MONOLITHIC_LIBRARY})
endif()

add_library(nsc_llvm STATIC
    src/LLVM/LlvmModule.cpp
    src/LLVM/LlvmOptimizationLevel.cpp
    src/LLVM/LlvmOptimizationObservation.cpp
    src/LLVM/LlvmProcessTrace.cpp
    src/LLVM/LlvmOptimizer.cpp
    src/LLVM/LlvmLowerer.cpp
    src/LLVM/ProjectOopLlvmLowerer.cpp
    src/LLVM/LlvmTargetMachine.cpp
    src/LLVM/CoffObjectVerifier.cpp)

target_include_directories(nsc_llvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_llvm PUBLIC nsc_frontend nsc_target PRIVATE ${NSC_LLVM_LINK_LIBRARIES})
target_compile_features(nsc_llvm PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_llvm)

add_library(nsc_runtime STATIC
    src/Runtime/WindowsRuntimeBuilder.cpp)

target_include_directories(nsc_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_runtime PUBLIC nsc_llvm)
target_compile_features(nsc_runtime PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_runtime)

add_library(nsc_linker STATIC
    src/Linker/LldLinker.cpp
    src/Linker/CoffArchiveVerifier.cpp
    src/Linker/PeExecutableVerifier.cpp
    src/Linker/WindowsExecutableBuilder.cpp
    src/Linker/WindowsLibraryBuilder.cpp
    src/Linker/WindowsRuntimeImports.cpp)

target_include_directories(nsc_linker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_linker PUBLIC nsc_runtime nsc_llvm)
target_compile_features(nsc_linker PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_linker)

add_library(nsc_driver STATIC
    src/Basic/Version.cpp
    src/Driver/Driver.cpp
    src/Interop/CHeaderGenerator.cpp
    src/Interop/LibraryMetadata.cpp)

target_include_directories(nsc_driver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_driver PUBLIC nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
target_compile_features(nsc_driver PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_driver)
add_dependencies(nsc_driver nsc_build_identity)

# nsc cae en build/bin/ -- CMAKE_RUNTIME_OUTPUT_DIRECTORY ya quedo en ${CMAKE_BINARY_DIR}/bin
# desde el bloque de arriba (justo despues de los option()), asi que no hace falta reasignarla
# aqui; solo cambia de nuevo mas abajo, justo antes del primer add_executable() de test.
add_executable(nsc src/nsc/main.cpp)
target_link_libraries(nsc PRIVATE nsc_driver)
target_compile_features(nsc PRIVATE cxx_std_20)
nsc_enable_warnings(nsc)

# "--deploy": instala el compilador ya construido en la ruta de instalacion elegida (p. ej.
# C:\native-sharp), junto con el arbol lib/ ENTERO -- las bibliotecas .ns
# (NativeSharp.Core/NativeSharp.Core.Threading/NativeSharp.Core.Collections.Generic/
# NativeSharp.Core.IO/NativeSharp.Core.Diagnostics) que todo proyecto NativeSharp consume via
# ProjectReference. NativeSharp no compila esas bibliotecas a un artefacto binario separado
# (siguen siendo codigo fuente .ns, recompilado por nsc en cada proyecto -- ver
# claude/05_metodo_de_trabajo_y_archivo_historico.md, "Convencion: el zip incremental siempre
# incluye lib/ completo" para el porque), asi que necesitan viajar junto al ejecutable, no solo
# el .exe solo, para que un proyecto pueda referenciarlas sin tener el repo entero al lado.
#
# Uso, DESPUES de un build normal (cmake -B build -S . ... && cmake --build build --config
# Release -j):
#   cmake --install build --config Release --prefix C:\native-sharp
#
# Deja <prefix>\bin\nsc.exe y <prefix>\lib\NativeSharp.Core\..., <prefix>\lib\
# NativeSharp.Core.Threading\..., etc. -- exactamente la forma que ya esperan como fallback fijo
# los build.bat de examples/tutorial_NN_*/ (buscan nsc.exe bajo C:\native-sharp\<build-dir>\bin\
# o C:\native-sharp\nsc.exe si no lo encuentran relativo al propio proyecto).
#
# RPATH (solo relevante en Linux/macOS -- PE/COFF de Windows no tiene concepto de RPATH, MSVC
# resuelve DLLs por orden de busqueda/PATH, asi que este bloque entero es un no-op ahi): por
# defecto `cmake --install` intenta REESCRIBIR el RPATH del binario instalado (de la ruta de
# build a otra cosa), y en este sandbox esa reescritura choca con un quirk de CMake al comparar
# el RPATH que arma en base a LLVM_LIBRARY_DIRS contra el que el linker realmente grabo en el
# binario. Se le pide a CMake CONSERVAR el mismo RPATH que ya usa el binario de build (apunta a
# LLVM_LIBRARY_DIRS, la carpeta que `find_package(LLVM CONFIG)` ya resolvio arriba, sea cual sea
# en la maquina que compile) en vez de reescribirlo -- ademas de evitar el quirk, es lo correcto:
# el binario instalado en Linux/macOS sigue necesitando encontrar la misma libLLVM del sistema.
if(NOT WIN32)
    set_target_properties(nsc PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${LLVM_LIBRARY_DIRS}")
endif()
install(TARGETS nsc RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/ DESTINATION lib)

# =====================================================================================
# NSC_TEST_COMPILER -- EL COMPILADOR QUE MIDE CTEST.
#
# Los 116 tests de mas abajo invocaban el OBJETIVO `nsc`, y CMake resuelve un objetivo al
# binario del ARBOL DE BUILD: build-clang/bin/nsc.exe. Eso ata la bateria a un artefacto
# intermedio en vez de al que se entrega. `cmake --install` copia ese binario a
# C:/native-sharp/bin, y entre los dos caben una instalacion que fallo, un install que se
# salto, o un arbol de build rehecho despues. Lo que se mide tiene que ser lo instalado,
# porque es lo que un proyecto de fuera va a usar.
#
# EL 2026-09-04 ESO SALIO A LA SUPERFICIE. test_nsc.bat construye el objetivo `pruebas`,
# que EXCLUYE `nsc` a proposito -- para no cambiar el compilador que se esta midiendo. Sin
# `build-clang/bin/nsc.exe`, 116 tests salieron `***Not Run` con "Could not find executable
# A:/nsc-devel/build-clang/bin/nsc.exe". El defecto no era el fichero ausente: era que la
# bateria apuntaba al sitio equivocado desde el principio, y solo se vio cuando el sitio
# equivocado quedo vacio.
#
# ES UNA RUTA Y NO UN OBJETIVO, Y ESO ES DELIBERADO. Mientras se escriba el nombre del
# objetivo, CMake tiene que resolverlo al arbol de build por definicion: no existe forma de
# que `COMMAND nsc` signifique "el instalado". Escribir la ruta rompe ese vinculo, que es
# justo lo que se quiere -- ctest ya no depende de construir nada y mide el fichero que hay.
#
# LA CONTRAPARTIDA, DICHA ENTERA: ctest deja de garantizar que ese fichero este al dia. Esa
# garantia se mueve a quien lanza la corrida, y hay dos sitios que la dan:
#
#   * all.bat instala en el paso 1, justo antes de ctest, asi que ahi es fresco por
#     construccion;
#   * test_nsc.bat no instala --es su promesa-- y por eso compara la fecha del ejecutable
#     con la de las fuentes (build_al_dia.py --solo-nsc) y avisa antes de medir nada.
#
# Y dentro de la propia bateria queda `nsc.version`, el primero de los 116: comprueba
# version y stage contra un regex, asi que un compilador de otra stage se pone rojo ahi.
#
# FUERA DE WINDOWS no existe C:/, asi que el valor por defecto vuelve a ser el del arbol de
# build. La variable es de cache: `-DNSC_TEST_COMPILER=<ruta>` manda sobre las dos ramas.
# =====================================================================================
set(NSC_TEST_COMPILER "" CACHE FILEPATH
    "Ejecutable nsc que mide ctest (vacio = C:/native-sharp/bin/nsc.exe en Windows)")
set(_nsc_test_compiler_es_ruta TRUE)
if(NSC_TEST_COMPILER STREQUAL "")
    if(WIN32)
        set(NSC_TEST_COMPILER "C:/native-sharp/bin/nsc.exe")
    else()
        set(NSC_TEST_COMPILER "$<TARGET_FILE:nsc>")
        set(_nsc_test_compiler_es_ruta FALSE)
    endif()
endif()
message(STATUS "ctest mide el compilador: ${NSC_TEST_COMPILER}")
if(_nsc_test_compiler_es_ruta AND NOT EXISTS "${NSC_TEST_COMPILER}")
    # NO es FATAL_ERROR, y la razon es el huevo y la gallina: en un arbol recien clonado el
    # configure ocurre ANTES del primer install, asi que parar aqui haria imposible la
    # primera construccion. Se avisa; si sigue faltando cuando ctest corra, lo repetira el
    # propio ctest, y ahora nombrando la ruta correcta.
    message(WARNING
        "NSC_TEST_COMPILER todavia no existe: ${NSC_TEST_COMPILER} -- los tests que lo "
        "invocan saldran `Not Run` hasta que se instale "
        "(cmake --install <build> --prefix C:/native-sharp)")
endif()

if(NSC_BUILD_TESTS)
    enable_testing()

    # A partir de aqui, todo add_executable() (via la funcion nsc_add_unit_test o directo) cae
    # en build/tests/ en vez de build/bin/ -- ver comentario arriba.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

    function(nsc_add_unit_test target_name source_file test_name)
        add_executable(${target_name} ${source_file})
        target_link_libraries(${target_name} PRIVATE nsc_frontend nsc_target)
        target_compile_features(${target_name} PRIVATE cxx_std_20)
        nsc_enable_warnings(${target_name})
        add_test(NAME ${test_name} COMMAND ${target_name})
    endfunction()

    add_executable(nsc_target_data_layout_tests tests/unit/TargetDataLayoutTests.cpp)
    target_link_libraries(nsc_target_data_layout_tests PRIVATE nsc_target)
    target_compile_features(nsc_target_data_layout_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_target_data_layout_tests)
    add_test(NAME nsc.target-data-layout COMMAND nsc_target_data_layout_tests)

    add_executable(nsc_compilation_model_tests tests/unit/CompilationModelTests.cpp)
    target_link_libraries(nsc_compilation_model_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_compilation_model_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_compilation_model_tests)
    add_test(NAME nsc.compilation-model COMMAND nsc_compilation_model_tests)

    add_executable(nsc_project_loader_tests tests/unit/ProjectLoaderTests.cpp)
    target_link_libraries(nsc_project_loader_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_project_loader_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_loader_tests)
    add_test(NAME nsc.project-loader COMMAND nsc_project_loader_tests)

    add_executable(nsc_project_graph_tests tests/unit/ProjectGraphTests.cpp)
    target_link_libraries(nsc_project_graph_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_project_graph_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_graph_tests)
    add_test(NAME nsc.project-graph COMMAND nsc_project_graph_tests)

    add_executable(nsc_project_compilation_tests tests/unit/ProjectCompilationTests.cpp)
    target_link_libraries(nsc_project_compilation_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_compilation_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_compilation_tests)
    add_test(NAME nsc.project-compilation COMMAND nsc_project_compilation_tests)

    add_executable(nsc_project_name_lookup_tests tests/unit/ProjectNameLookupTests.cpp)
    target_link_libraries(nsc_project_name_lookup_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_name_lookup_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_name_lookup_tests)
    add_test(NAME nsc.project-name-lookup COMMAND nsc_project_name_lookup_tests)

    nsc_add_unit_test(nsc_source_manager_tests tests/unit/SourceManagerTests.cpp nsc.source-manager)
    nsc_add_unit_test(nsc_diagnostic_tests tests/unit/DiagnosticTests.cpp nsc.diagnostics)
    nsc_add_unit_test(nsc_lexer_tests tests/unit/LexerTests.cpp nsc.lexer)
    nsc_add_unit_test(nsc_parser_tests tests/unit/ParserTests.cpp nsc.parser)
    nsc_add_unit_test(nsc_namespace_syntax_tests tests/unit/NamespaceSyntaxTests.cpp nsc.namespace-syntax)
    nsc_add_unit_test(nsc_class_declaration_tests tests/unit/ClassDeclarationTests.cpp nsc.class-declarations)
    nsc_add_unit_test(nsc_class_member_syntax_tests tests/unit/ClassMemberSyntaxTests.cpp nsc.class-member-syntax)
    nsc_add_unit_test(nsc_constructor_syntax_tests tests/unit/ConstructorSyntaxTests.cpp nsc.constructor-syntax)

    add_executable(nsc_partial_class_aggregation_tests tests/unit/PartialClassAggregationTests.cpp)
    target_link_libraries(nsc_partial_class_aggregation_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_partial_class_aggregation_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_partial_class_aggregation_tests)
    add_test(NAME nsc.partial-class-aggregation COMMAND nsc_partial_class_aggregation_tests)

    add_executable(nsc_project_member_model_tests tests/unit/ProjectMemberModelTests.cpp)
    target_link_libraries(nsc_project_member_model_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_member_model_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_member_model_tests)
    add_test(NAME nsc.project-member-model COMMAND nsc_project_member_model_tests)

    nsc_add_unit_test(
        nsc_project_type_conversion_tests
        tests/unit/ProjectTypeConversionTests.cpp
        nsc.project-type-conversion)

    add_executable(nsc_project_method_body_model_tests tests/unit/ProjectMethodBodyModelTests.cpp)
    target_link_libraries(nsc_project_method_body_model_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_method_body_model_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_method_body_model_tests)
    add_test(NAME nsc.project-method-body-model COMMAND nsc_project_method_body_model_tests)

    add_executable(nsc_project_construction_model_tests tests/unit/ProjectConstructionModelTests.cpp)
    target_link_libraries(nsc_project_construction_model_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_construction_model_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_construction_model_tests)
    add_test(NAME nsc.project-construction-model COMMAND nsc_project_construction_model_tests)

    add_executable(nsc_project_object_layout_model_tests tests/unit/ProjectObjectLayoutModelTests.cpp)
    target_link_libraries(nsc_project_object_layout_model_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_project_object_layout_model_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_object_layout_model_tests)
    add_test(NAME nsc.project-object-layout-model COMMAND nsc_project_object_layout_model_tests)

    add_executable(nsc_project_oop_lowering_tests tests/unit/ProjectOopLoweringTests.cpp)
    target_link_libraries(nsc_project_oop_lowering_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_project_oop_lowering_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_project_oop_lowering_tests)
    add_test(NAME nsc.project-oop-lowering COMMAND nsc_project_oop_lowering_tests)

    add_executable(nsc_macro_a_oop_tests tests/unit/MacroAOopTests.cpp)
    target_link_libraries(nsc_macro_a_oop_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_macro_a_oop_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_macro_a_oop_tests)
    add_test(NAME nsc.macro-a-oop COMMAND nsc_macro_a_oop_tests)

    add_executable(nsc_stage37_static_initialization_tests tests/unit/Stage37StaticInitializationTests.cpp)
    target_link_libraries(nsc_stage37_static_initialization_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage37_static_initialization_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage37_static_initialization_tests)
    add_test(NAME nsc.stage37.static-initialization COMMAND nsc_stage37_static_initialization_tests)

    add_executable(nsc_stage35_recovery_tests tests/unit/Stage35RecoveryTests.cpp)
    target_link_libraries(nsc_stage35_recovery_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage35_recovery_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage35_recovery_tests)
    add_test(NAME nsc.stage35.recovery COMMAND nsc_stage35_recovery_tests)

    add_executable(nsc_stage36_recovery_tests tests/unit/Stage36RecoveryTests.cpp)
    target_link_libraries(nsc_stage36_recovery_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage36_recovery_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage36_recovery_tests)
    add_test(NAME nsc.stage36.recovery COMMAND nsc_stage36_recovery_tests)

    add_executable(nsc_stage38_properties_indexers_operators_tests
        tests/unit/Stage38PropertiesIndexersOperatorsTests.cpp)
    target_link_libraries(nsc_stage38_properties_indexers_operators_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage38_properties_indexers_operators_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage38_properties_indexers_operators_tests)
    add_test(NAME nsc.stage38.properties-indexers-operators
        COMMAND nsc_stage38_properties_indexers_operators_tests)

    add_executable(nsc_stage39_rtti_casts_tests tests/unit/Stage39RttiCastsTests.cpp)
    target_link_libraries(nsc_stage39_rtti_casts_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage39_rtti_casts_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage39_rtti_casts_tests)
    add_test(NAME nsc.stage39.rtti-casts COMMAND nsc_stage39_rtti_casts_tests)

    add_executable(nsc_stage40_struct_value_abi_tests tests/unit/Stage40StructValueAbiTests.cpp)
    target_link_libraries(nsc_stage40_struct_value_abi_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage40_struct_value_abi_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage40_struct_value_abi_tests)
    add_test(NAME nsc.stage40.struct-value-abi COMMAND nsc_stage40_struct_value_abi_tests)

    add_executable(nsc_stage41_arrays_strings_core_tests tests/unit/Stage41ArraysStringsCoreTests.cpp)
    target_link_libraries(nsc_stage41_arrays_strings_core_tests PRIVATE nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage41_arrays_strings_core_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage41_arrays_strings_core_tests)
    add_test(NAME nsc.stage41.arrays-strings-core COMMAND nsc_stage41_arrays_strings_core_tests)

    add_executable(nsc_stage42_delegates_events_tests tests/unit/Stage42DelegatesEventsTests.cpp)
    target_link_libraries(nsc_stage42_delegates_events_tests PRIVATE nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage42_delegates_events_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage42_delegates_events_tests)
    add_test(NAME nsc.stage42.delegates-events COMMAND nsc_stage42_delegates_events_tests)

    add_executable(nsc_stage43_lambdas_closures_tests tests/unit/Stage43LambdasClosuresTests.cpp)
    target_link_libraries(nsc_stage43_lambdas_closures_tests PRIVATE nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage43_lambdas_closures_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage43_lambdas_closures_tests)
    add_test(NAME nsc.stage43.lambdas-closures COMMAND nsc_stage43_lambdas_closures_tests)

    add_executable(nsc_stage44_generics_monomorphization_tests tests/unit/Stage44GenericsMonomorphizationTests.cpp)
    target_link_libraries(nsc_stage44_generics_monomorphization_tests PRIVATE nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage44_generics_monomorphization_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage44_generics_monomorphization_tests)
    add_test(NAME nsc.stage44.generics-monomorphization COMMAND nsc_stage44_generics_monomorphization_tests)

    add_executable(nsc_stage45_exceptions_syntax_tests tests/unit/Stage45ExceptionsSyntaxTests.cpp)
    target_link_libraries(nsc_stage45_exceptions_syntax_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_exceptions_syntax_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_exceptions_syntax_tests)
    add_test(NAME nsc.stage45.exceptions-syntax COMMAND nsc_stage45_exceptions_syntax_tests)

    add_executable(nsc_stage45_exception_sema_tests tests/unit/Stage45ExceptionSemaTests.cpp)
    target_link_libraries(nsc_stage45_exception_sema_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_exception_sema_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_exception_sema_tests)
    add_test(NAME nsc.stage45.exception-sema COMMAND nsc_stage45_exception_sema_tests)

    add_executable(nsc_stage45_cleanup_scope_tests tests/unit/Stage45CleanupScopeTests.cpp)
    target_link_libraries(nsc_stage45_cleanup_scope_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_cleanup_scope_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_cleanup_scope_tests)
    add_test(NAME nsc.stage45.cleanup-scopes COMMAND nsc_stage45_cleanup_scope_tests)

    add_executable(nsc_stage45_finally_flow_tests tests/unit/Stage45FinallyFlowTests.cpp)
    target_link_libraries(nsc_stage45_finally_flow_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_finally_flow_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_finally_flow_tests)
    add_test(NAME nsc.stage45.finally-flow COMMAND nsc_stage45_finally_flow_tests)

    add_executable(nsc_stage45_using_disposable_tests tests/unit/Stage45UsingDisposableTests.cpp)
    target_link_libraries(nsc_stage45_using_disposable_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_using_disposable_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_using_disposable_tests)
    add_test(NAME nsc.stage45.using-disposable COMMAND nsc_stage45_using_disposable_tests)

    add_executable(nsc_stage45_hir_exceptional_cfg_tests tests/unit/Stage45HirExceptionalCfgTests.cpp)
    target_link_libraries(nsc_stage45_hir_exceptional_cfg_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_hir_exceptional_cfg_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_hir_exceptional_cfg_tests)
    add_test(NAME nsc.stage45.hir-exceptional-cfg COMMAND nsc_stage45_hir_exceptional_cfg_tests)

    add_executable(nsc_stage45_mir_exceptional_cfg_tests tests/unit/Stage45MirExceptionalCfgTests.cpp)
    target_link_libraries(nsc_stage45_mir_exceptional_cfg_tests PRIVATE nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage45_mir_exceptional_cfg_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_mir_exceptional_cfg_tests)
    add_test(NAME nsc.stage45.mir-exceptional-cfg COMMAND nsc_stage45_mir_exceptional_cfg_tests)

    add_executable(nsc_stage45_native_exception_runtime_tests tests/unit/Stage45NativeExceptionRuntimeTests.cpp)
    target_link_libraries(nsc_stage45_native_exception_runtime_tests PRIVATE nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_native_exception_runtime_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_native_exception_runtime_tests)
    add_test(NAME nsc.stage45.native-exception-runtime COMMAND nsc_stage45_native_exception_runtime_tests)

    add_executable(nsc_stage45_llvm_win64_eh_basic_tests tests/unit/Stage45LlvmWin64EhBasicTests.cpp)
    target_link_libraries(nsc_stage45_llvm_win64_eh_basic_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_llvm_win64_eh_basic_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_llvm_win64_eh_basic_tests)
    add_test(NAME nsc.stage45.llvm-win64-eh-basic COMMAND nsc_stage45_llvm_win64_eh_basic_tests)

    add_executable(nsc_stage45_llvm_cleanup_funclet_tests tests/unit/Stage45LlvmCleanupFuncletTests.cpp)
    target_link_libraries(nsc_stage45_llvm_cleanup_funclet_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_llvm_cleanup_funclet_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_llvm_cleanup_funclet_tests)
    add_test(NAME nsc.stage45.llvm-cleanup-funclets COMMAND nsc_stage45_llvm_cleanup_funclet_tests)

    add_executable(nsc_stage45_typed_catch_rtti_tests tests/unit/Stage45TypedCatchRttiTests.cpp)
    target_link_libraries(nsc_stage45_typed_catch_rtti_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_typed_catch_rtti_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_typed_catch_rtti_tests)
    add_test(NAME nsc.stage45.typed-catch-rtti COMMAND nsc_stage45_typed_catch_rtti_tests)

    add_executable(nsc_stage45_linker_pe_eh_tests tests/unit/Stage45LinkerPeEhTests.cpp)
    target_link_libraries(nsc_stage45_linker_pe_eh_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_linker_pe_eh_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_linker_pe_eh_tests)
    add_test(NAME nsc.stage45.linker-pe-eh COMMAND nsc_stage45_linker_pe_eh_tests)

    # Bug B-001 fix regression suite (EH+RAII reuse-after-catch SIGSEGV/access-violation) --
    # see Stage45_10EhRaiiReuseAfterCatchTests.cpp's own header comment for the full root cause.
    add_executable(nsc_stage45_10_eh_raii_reuse_after_catch_tests tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp)
    target_link_libraries(nsc_stage45_10_eh_raii_reuse_after_catch_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_10_eh_raii_reuse_after_catch_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_10_eh_raii_reuse_after_catch_tests)
    add_test(NAME nsc.stage45-10.eh-raii-reuse-after-catch COMMAND nsc_stage45_10_eh_raii_reuse_after_catch_tests)

    # Cleanup EXECUTION suite -- every scope-exit obligation on an exceptional path (`finally`,
    # `using`/Dispose, `owned` parameter, `raii`, rethrow) verified by RUNNING a real PE and
    # COUNTING how many times each cleanup body actually ran, at -O0 and -O2. Exists because an
    # audit found only 27 of 112 unit-test files execute anything; see this file's own header for
    # which shape-only assertions elsewhere it supplies the missing half of.
    add_executable(nsc_stage45_11_eh_cleanup_execution_tests tests/unit/Stage45_11EhCleanupExecutionTests.cpp)
    target_link_libraries(nsc_stage45_11_eh_cleanup_execution_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage45_11_eh_cleanup_execution_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage45_11_eh_cleanup_execution_tests)
    add_test(NAME nsc.stage45-11.eh-cleanup-execution COMMAND nsc_stage45_11_eh_cleanup_execution_tests)

    # Bloque 1 (Ownership soundness) -- ownership EXECUTION suite. Verifies every ownership
    # obligation by RUNNING a real PE and COUNTING destructions at -O0 and -O2, plus the negative
    # half (programs that must be rejected). Exists because Stage 50's ownership work was covered
    # exclusively by IR-shape assertions, which cannot tell whether an owner is ever destroyed;
    # probing by execution found four shipping defects. See the file's own header.
    add_executable(nsc_stage50_11_ownership_execution_tests tests/unit/Stage50_11OwnershipExecutionTests.cpp)
    target_link_libraries(nsc_stage50_11_ownership_execution_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_11_ownership_execution_tests PRIVATE cxx_std_20)
    # Bloque 1 owning-slot closure: the owned-string overwrite scenario references the real
    # NativeSharp.Core project (String.Copy is the owned-returning allocation a string slot needs
    # once literals -- constant globals -- are rejected as owning-store RHS).
    target_compile_definitions(nsc_stage50_11_ownership_execution_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage50_11_ownership_execution_tests)
    add_test(NAME nsc.stage50-11.ownership-execution COMMAND nsc_stage50_11_ownership_execution_tests)

    # Stage 40.2 -- Win64 aggregate ABI (indirect struct passing/sret), verified by real
    # execution under Wine at -O0 and -O2. Ground truth: clang for x86_64-pc-windows-msvc.
    add_executable(nsc_stage40_2_aggregate_abi_execution_tests tests/unit/Stage40_2AggregateAbiExecutionTests.cpp)
    target_link_libraries(nsc_stage40_2_aggregate_abi_execution_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage40_2_aggregate_abi_execution_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage40_2_aggregate_abi_execution_tests)
    add_test(NAME nsc.stage40-2.aggregate-abi-execution COMMAND nsc_stage40_2_aggregate_abi_execution_tests)

    # Stage 55.x -- Interop Foundation: generic FFI imports (dynamic + static-resolved),
    # --link-lib, OOP-pipeline pointers, extern C delegate creation, and the cross-host
    # certification (clang MSVC-ABI host exchanging structs with and calling back into a
    # NativeSharp static library, executing under Wine).
    add_executable(nsc_stage55x_interop_foundation_tests tests/unit/Stage55XInteropFoundationTests.cpp)
    target_link_libraries(nsc_stage55x_interop_foundation_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage55x_interop_foundation_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage55x_interop_foundation_tests)
    add_test(NAME nsc.stage55x.interop-foundation COMMAND nsc_stage55x_interop_foundation_tests)

    # Stage 56 / C1 "Interop completion" -- the three gaps Stage 55.x left named in the compiler
    # roadmap: the function-pointer round trip (nint/void* <-> extern C function pointer, function
    # pointer returns, explicit reinterpretation between foreign signatures), import identity as
    # the triple (library, symbol, signature), and the callback exception boundary. Needs
    # NSC_SOURCE_DIR because it also compiles the tests/interop_contract/ fixtures from the source
    # tree, not only its own inline sources.
    add_executable(nsc_stage56_c1_interop_completion_tests tests/unit/Stage56C1InteropCompletionTests.cpp)
    target_link_libraries(nsc_stage56_c1_interop_completion_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_c1_interop_completion_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_c1_interop_completion_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_c1_interop_completion_tests)
    add_test(NAME nsc.stage56.c1-interop-completion COMMAND nsc_stage56_c1_interop_completion_tests)

    # Stage 56 / C2-C3-C4 "Language completion" -- C aggregate completeness (fixed arrays, union,
    # pack/align/explicit offsets), FFI aliases, and ref/out/in. Needs NSC_SOURCE_DIR: besides its
    # own inline sources it compiles the tests/interop_contract/ fixtures and the packaged example
    # straight out of the source tree, and it drives clang over the generated host header so an
    # independent toolchain certifies every layout number.
    add_executable(nsc_stage56_c2c4_language_completion_tests tests/unit/Stage56C2C4LanguageCompletionTests.cpp)
    target_link_libraries(nsc_stage56_c2c4_language_completion_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_c2c4_language_completion_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_c2c4_language_completion_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_c2c4_language_completion_tests)
    add_test(NAME nsc.stage56.c2c4-language-completion COMMAND nsc_stage56_c2c4_language_completion_tests)

    # Stage 56 / C6 "Struct members" -- constructors, properties, indexers and operators on
    # structs, plus the two members that stay refused (destructors and virtual) with diagnostics
    # that say why. Needs NSC_SOURCE_DIR: it compiles the tests/interop_contract/ math fixture and
    # the packaged example from the source tree, and drives clang over the generated host header
    # to certify that none of these members changed the struct's layout or ABI.
    add_executable(nsc_stage56_c6_struct_members_tests tests/unit/Stage56C6StructMembersTests.cpp)
    target_link_libraries(nsc_stage56_c6_struct_members_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_c6_struct_members_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_c6_struct_members_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_c6_struct_members_tests)
    add_test(NAME nsc.stage56.c6-struct-members COMMAND nsc_stage56_c6_struct_members_tests)

    # Stage 56 / C7 -- definite assignment and the aggregate initializer. Closes the largest
    # silent-wrong-answer hole the language had (a local declared without an initializer used to
    # be an alloca and nothing else) with two halves: defined zero storage in the backend, and a
    # path-sensitive MUST-analysis in the front end. Checks IR shape, real executables at -O0 and
    # -O2, the exact set of programs the analysis rejects, and -- twice -- that -O2 folds the whole
    # body to a constant return, which is what proves the defaulting stores cost nothing.
    add_executable(nsc_stage56_c7_definite_assignment_tests tests/unit/Stage56C7DefiniteAssignmentTests.cpp)
    target_link_libraries(nsc_stage56_c7_definite_assignment_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_c7_definite_assignment_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_c7_definite_assignment_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_c7_definite_assignment_tests)
    add_test(NAME nsc.stage56.c7-definite-assignment COMMAND nsc_stage56_c7_definite_assignment_tests)

    # Stage 56 / C8 -- the type root. NativeSharp.Core.Object as the real base of every class
    # (ToString/Equals/GetHashCode, virtual, with the runtime type name carried in the class
    # descriptor), the same three synthesized for structs and resolved statically, and NO boxing
    # anywhere. Checks IR shape, real executables, the -O2 fold of the struct half, the exact set
    # of programs the design refuses, and that a project without the root in view is untouched.
    add_executable(nsc_stage56_c8_type_root_tests tests/unit/Stage56C8TypeRootTests.cpp)
    target_link_libraries(nsc_stage56_c8_type_root_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_c8_type_root_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_c8_type_root_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_c8_type_root_tests)
    add_test(NAME nsc.stage56.c8-type-root COMMAND nsc_stage56_c8_type_root_tests)

    add_executable(nsc_stage56_bugfix_tests tests/unit/Stage56BugFixTests.cpp)
    target_link_libraries(nsc_stage56_bugfix_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage56_bugfix_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage56_bugfix_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage56_bugfix_tests)
    add_test(NAME nsc.stage56.bugfix COMMAND nsc_stage56_bugfix_tests)

    add_executable(nsc_stage57_language_tests tests/unit/Stage57LanguageTests.cpp)
    target_link_libraries(nsc_stage57_language_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage57_language_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage57_language_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage57_language_tests)
    add_test(NAME nsc.stage57.language COMMAND nsc_stage57_language_tests)

    # ---------------------------------------------------------------------------------------
    # Milestone CC1 -- Compiler Correctness & Semantic Consolidation.
    # Ver MILESTONE_1_TASKS.md para el registro de defectos y el estado de cada tarea.
    # ---------------------------------------------------------------------------------------

    # CC1-000/CC1-001: identidad del build. Solo necesita el header generado, sin frontend.
    nsc_add_unit_test(
        nsc_cc1_build_identity_tests
        tests/unit/Cc1BuildIdentityTests.cpp
        nsc.version.identity)
    target_link_libraries(nsc_cc1_build_identity_tests PRIVATE nsc_driver)

    # CC1-102/CC1-103/CC1-104: el evaluador entero tipado compartido. Unit test puro.
    nsc_add_unit_test(
        nsc_cc1_integer_evaluator_tests
        tests/unit/Cc1IntegerEvaluatorTests.cpp
        nsc.cc1.integer-evaluator)

    # CC1-002: la suite de fixtures del milestone -- positivas de ejecucion y negativas de
    # diagnostico. Necesita el driver completo porque compila y enlaza proyectos reales.
    add_executable(nsc_cc1_correctness_tests tests/unit/Cc1CorrectnessTests.cpp)
    target_link_libraries(nsc_cc1_correctness_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_cc1_correctness_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_cc1_correctness_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_cc1_correctness_tests)

    # UNA PRUEBA DE ctest POR FIXTURE -- 2026-09-03.
    #
    # POR QUE. En la corrida del 2026-09-03, `ctest -j 16` tardo 274.47 s de reloj y esta suite
    # tardo 273.97 s de esos: el reloj de ctest ERA esta prueba, con las otras quince CPU paradas.
    # Compilaba y ejecutaba treinta y seis fixtures EN SERIE dentro de un solo proceso. Repartidas,
    # el techo pasa a ser la siguiente prueba mas lenta (nsc.stage56.c8-type-root, 90.8 s).
    #
    # Y el segundo motivo, que no es velocidad: un rojo decia `nsc.cc1.correctness` y habia que
    # abrir el log para saber QUE fixture fallo. Ahora lo dice el nombre de la prueba.
    #
    # LA LISTA SE LEE DEL PROPIO .cpp, no se escribe aqui. Escribirla seria la quinta copia de una
    # lista en este proyecto, y las cuatro anteriores costaron un documento cada una: la tabla de
    # imports (docs 102/103), las tablas de enteros (112), las cadenas del HIR (106) y las dos
    # tablas de casos (115). Anadir una fixture sigue siendo UNA linea, en el .cpp, como siempre.
    #
    # DOS FORMAS DE QUEDARSE OBSOLETA, Y LAS DOS ESTAN CERRADAS ABAJO:
    #
    #   1. La lista se lee en tiempo de CONFIGURE. Sin declarar el .cpp como dependencia del
    #      configure, anadir una fixture y compilar NO registraria su prueba -- la corrida saldria
    #      verde midiendo una fixture menos, que es la peor forma de fallar que tiene una suite.
    #      CMAKE_CONFIGURE_DEPENDS lo arregla: tocar el fichero vuelve a correr el configure.
    #   2. El umbral era `LESS 30`, un numero escrito a mano cuando habia treinta y seis fixtures.
    #      Un suelo fijo ENVEJECE: con treinta y ocho en el arbol, un regex roto que reconociera
    #      treinta y una pasaria el guardia y se perderian siete sin decir nada.
    #
    # EL GUARDIA COMPARA CONTRA EL DIRECTORIO, que es la unica fuente de verdad que no se puede
    # olvidar de actualizar: TODA carpeta de tests/cc1 con un .nsproj tiene que tener su prueba
    # registrada. Esto no envejece, y ademas caza el fallo que de verdad ocurrio dos veces el
    # 2026-09-03 -- una fixture escrita y nunca inscrita (c37), que no es un regex roto sino un
    # olvido, y que un suelo numerico no habria detectado jamas.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/Cc1CorrectnessTests.cpp")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/Cc1CorrectnessTests.cpp" _cc1_source)
    string(REGEX MATCHALL "checkFixture\\([^;]*;" _cc1_calls "${_cc1_source}")
    set(_cc1_labels "")
    foreach(_cc1_call IN LISTS _cc1_calls)
        if(_cc1_call MATCHES "\"(cc1-[a-z0-9-]+)\"")
            list(APPEND _cc1_labels "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _cc1_labels)
    list(LENGTH _cc1_labels _cc1_count)

    # `c38_enum_ancho/c38_enum_ancho.nsproj` -> `cc1-c38-enum-ancho`.
    #
    # CONFIGURE_DEPENDS NO ES OPCIONAL AQUI. Un `file(GLOB)` normal se evalua UNA VEZ, en el
    # configure, y despues nada vuelve a mirar el directorio: crear una fixture nueva no despertaria
    # al guardia hasta que otra cosa provocara un reconfigure. O sea que el guardia escrito para
    # cazar "una fixture escrita y nunca inscrita" no habria visto precisamente el caso recien
    # escrito -- que es el unico momento en que ese olvido ocurre. Con CONFIGURE_DEPENDS, cmake
    # comprueba el glob en cada build y vuelve a configurar si cambio.
    file(GLOB _cc1_projects CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cc1/*/*.nsproj")
    set(_cc1_missing "")
    set(_cc1_fixture_count 0)
    foreach(_cc1_project IN LISTS _cc1_projects)
        get_filename_component(_cc1_dir "${_cc1_project}" DIRECTORY)
        get_filename_component(_cc1_dir "${_cc1_dir}" NAME)
        string(REPLACE "_" "-" _cc1_dir "${_cc1_dir}")
        math(EXPR _cc1_fixture_count "${_cc1_fixture_count} + 1")
        if(NOT "cc1-${_cc1_dir}" IN_LIST _cc1_labels)
            list(APPEND _cc1_missing "cc1-${_cc1_dir}")
        endif()
    endforeach()
    if(_cc1_missing)
        message(FATAL_ERROR
            "tests/cc1 tiene fixtures que Cc1CorrectnessTests.cpp no registra: ${_cc1_missing}. "
            "Una fixture escrita y no inscrita no mide nada, y la corrida sale verde igual")
    endif()
    if(_cc1_count LESS _cc1_fixture_count)
        # Si el regex deja de encontrar las llamadas --porque alguien cambio la forma de
        # checkFixture-- esto NO puede quedarse en silencio registrando tres pruebas: seria una
        # corrida verde que mide una decima parte. Se para el configure.
        message(FATAL_ERROR
            "Cc1CorrectnessTests.cpp: solo se reconocieron ${_cc1_count} etiquetas para "
            "${_cc1_fixture_count} fixtures en el arbol; el patron de checkFixture cambio y esta "
            "lista dejaria de medir el corpus entero")
    endif()
    foreach(_cc1_label IN LISTS _cc1_labels)
        add_test(NAME nsc.cc1.correctness.${_cc1_label}
                 COMMAND nsc_cc1_correctness_tests ${_cc1_label})
    endforeach()
    # Y las comprobaciones de IR, que no compilan fixtures del arbol.
    add_test(NAME nsc.cc1.correctness COMMAND nsc_cc1_correctness_tests --rest)

    add_executable(nsc_stage46_native_libraries_tests tests/unit/Stage46NativeLibrariesTests.cpp)
    target_link_libraries(nsc_stage46_native_libraries_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage46_native_libraries_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage46_native_libraries_tests)
    add_test(NAME nsc.stage46.native-libraries COMMAND nsc_stage46_native_libraries_tests)

    add_executable(nsc_stage47_host_c_abi_tests tests/unit/Stage47HostCAbiTests.cpp)
    target_link_libraries(nsc_stage47_host_c_abi_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage47_host_c_abi_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage47_host_c_abi_tests)
    add_test(NAME nsc.stage47.host-c-abi COMMAND nsc_stage47_host_c_abi_tests)

    add_executable(nsc_stage48_1_c_header_tests tests/unit/Stage48_1CHeaderGeneratorTests.cpp)
    target_link_libraries(nsc_stage48_1_c_header_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_1_c_header_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_1_c_header_tests)
    add_test(NAME nsc.stage48_1.c-header COMMAND nsc_stage48_1_c_header_tests)

    add_executable(nsc_stage48_2_library_metadata_tests tests/unit/Stage48_2LibraryMetadataTests.cpp)
    target_link_libraries(nsc_stage48_2_library_metadata_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_2_library_metadata_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_2_library_metadata_tests)
    add_test(NAME nsc.stage48_2.library-metadata COMMAND nsc_stage48_2_library_metadata_tests)

    add_executable(nsc_stage48_3_c_host_tests tests/unit/Stage48_3CHostIntegrationTests.cpp)
    target_link_libraries(nsc_stage48_3_c_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_3_c_host_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_3_c_host_tests)
    add_test(NAME nsc.stage48_3.c-host COMMAND nsc_stage48_3_c_host_tests)

    add_executable(nsc_stage48_4_cpp_host_tests tests/unit/Stage48_4CppHostIntegrationTests.cpp)
    target_link_libraries(nsc_stage48_4_cpp_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_4_cpp_host_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_4_cpp_host_tests)
    add_test(NAME nsc.stage48_4.cpp-host COMMAND nsc_stage48_4_cpp_host_tests)

    add_executable(nsc_stage48_5_csharp_host_tests tests/unit/Stage48_5CSharpPInvokeHostTests.cpp)
    target_link_libraries(nsc_stage48_5_csharp_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_5_csharp_host_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_5_csharp_host_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage48_5_csharp_host_tests)
    add_test(NAME nsc.stage48_5.csharp-pinvoke-host COMMAND nsc_stage48_5_csharp_host_tests)

    add_executable(nsc_stage48_6_cppcli_host_tests tests/unit/Stage48_6CppCliHostClosureTests.cpp)
    target_link_libraries(nsc_stage48_6_cppcli_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_6_cppcli_host_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_6_cppcli_host_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage48_6_cppcli_host_tests)
    add_test(NAME nsc.stage48_6.cppcli-stage48-closure COMMAND nsc_stage48_6_cppcli_host_tests)

    add_executable(nsc_stage49_raii_move_tests tests/unit/Stage49RaiiMoveSemanticsTests.cpp)
    target_link_libraries(nsc_stage49_raii_move_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage49_raii_move_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage49_raii_move_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage49_raii_move_tests)
    add_test(NAME nsc.stage49.raii-move COMMAND nsc_stage49_raii_move_tests)

    add_executable(nsc_stage50_1_ownership_syntax_tests tests/unit/Stage50_1OwnershipSyntaxTests.cpp)
    target_link_libraries(nsc_stage50_1_ownership_syntax_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_1_ownership_syntax_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_1_ownership_syntax_tests)
    add_test(NAME nsc.stage50_1.ownership-syntax COMMAND nsc_stage50_1_ownership_syntax_tests)

    add_executable(nsc_stage50_2_ownership_type_system_tests tests/unit/Stage50_2OwnershipTypeSystemTests.cpp)
    target_link_libraries(nsc_stage50_2_ownership_type_system_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_2_ownership_type_system_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_2_ownership_type_system_tests)
    add_test(NAME nsc.stage50_2.ownership-type-system COMMAND nsc_stage50_2_ownership_type_system_tests)

    add_executable(nsc_stage50_3_owned_parameters_returns_tests tests/unit/Stage50_3OwnedParametersReturnsTests.cpp)
    target_link_libraries(nsc_stage50_3_owned_parameters_returns_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_3_owned_parameters_returns_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_3_owned_parameters_returns_tests)
    add_test(NAME nsc.stage50_3.owned-parameters-returns COMMAND nsc_stage50_3_owned_parameters_returns_tests)

    add_executable(nsc_stage50_4_borrowed_parameters_tests tests/unit/Stage50_4BorrowedParametersTests.cpp)
    target_link_libraries(nsc_stage50_4_borrowed_parameters_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_4_borrowed_parameters_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_4_borrowed_parameters_tests)
    add_test(NAME nsc.stage50_4.borrowed-parameters COMMAND nsc_stage50_4_borrowed_parameters_tests)

    add_executable(nsc_stage50_5_local_borrows_lifetimes_tests tests/unit/Stage50_5LocalBorrowsLifetimesTests.cpp)
    target_link_libraries(nsc_stage50_5_local_borrows_lifetimes_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_5_local_borrows_lifetimes_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_5_local_borrows_lifetimes_tests)
    add_test(NAME nsc.stage50_5.local-borrows-lifetimes COMMAND nsc_stage50_5_local_borrows_lifetimes_tests)

    add_executable(nsc_stage50_6_ownership_control_flow_tests tests/unit/Stage50_6OwnershipControlFlowTests.cpp)
    target_link_libraries(nsc_stage50_6_ownership_control_flow_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_6_ownership_control_flow_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_6_ownership_control_flow_tests)
    add_test(NAME nsc.stage50_6.ownership-control-flow COMMAND nsc_stage50_6_ownership_control_flow_tests)

    add_executable(nsc_stage50_7_borrow_escape_analysis_tests tests/unit/Stage50_7BorrowEscapeAnalysisTests.cpp)
    target_link_libraries(nsc_stage50_7_borrow_escape_analysis_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_7_borrow_escape_analysis_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_7_borrow_escape_analysis_tests)
    add_test(NAME nsc.stage50_7.borrow-escape-analysis COMMAND nsc_stage50_7_borrow_escape_analysis_tests)

    # Bloque 3 / SHR-013 -- "vistas y tiempo de vida". Es el hermano del 50.7 de arriba y va
    # pegado a el a proposito: los dos miden la MISMA propiedad --que nada prestado sobreviva a su
    # origen-- por dos caminos distintos. Alli el prestamo lo dice el calificativo `borrowed`
    # sobre un tipo por referencia; aqui lo dice el TIPO de un valor por valor, que es justo lo
    # que la comprobacion de 50.7 no podia ver.
    add_executable(nsc_stage58_view_lifetimes_tests tests/unit/Stage58ViewLifetimesTests.cpp)
    target_link_libraries(nsc_stage58_view_lifetimes_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage58_view_lifetimes_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage58_view_lifetimes_tests)
    add_test(NAME nsc.stage58.view-lifetimes COMMAND nsc_stage58_view_lifetimes_tests)

    # ID 131 -- miembros estaticos de un tipo generico CERRADO (`Caja<int>.Make(7)`). Va pegado a
    # la prueba del Bloque 3 a proposito: el hueco se descubrio implementandolo, y se rodeo alli
    # metiendo las factorias en clases no genericas. Esta prueba mide el arreglo Y, sobre todo,
    # que la desambiguacion entre "menor que" y "lista de argumentos de tipo" siga distinguiendo
    # las dos -- que es por donde un cambio en esto se lleva por delante la aritmetica de
    # comparaciones sin que nadie lo note.
    add_executable(nsc_stage59_closed_generic_member_tests tests/unit/Stage59ClosedGenericMemberTests.cpp)
    target_link_libraries(nsc_stage59_closed_generic_member_tests PRIVATE nsc_frontend nsc_target)
    target_compile_features(nsc_stage59_closed_generic_member_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage59_closed_generic_member_tests)
    add_test(NAME nsc.stage59.closed-generic-member COMMAND nsc_stage59_closed_generic_member_tests)

    add_executable(nsc_stage50_8_ownership_ir_lowering_tests tests/unit/Stage50_8OwnershipIrLoweringTests.cpp)
    target_link_libraries(nsc_stage50_8_ownership_ir_lowering_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_8_ownership_ir_lowering_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_8_ownership_ir_lowering_tests)
    add_test(NAME nsc.stage50_8.ownership-ir-lowering COMMAND nsc_stage50_8_ownership_ir_lowering_tests)

    add_executable(nsc_stage50_9_metadata_host_compatibility_tests tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp)
    target_link_libraries(nsc_stage50_9_metadata_host_compatibility_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_9_metadata_host_compatibility_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage50_9_metadata_host_compatibility_tests)
    add_test(NAME nsc.stage50_9.metadata-host-compatibility COMMAND nsc_stage50_9_metadata_host_compatibility_tests)

    add_executable(nsc_stage50_10_ownership_examples_documentation_tests tests/unit/Stage50_10OwnershipExamplesDocumentationTests.cpp)
    target_link_libraries(nsc_stage50_10_ownership_examples_documentation_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage50_10_ownership_examples_documentation_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage50_10_ownership_examples_documentation_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage50_10_ownership_examples_documentation_tests)
    add_test(NAME nsc.stage50_10.ownership-examples-documentation COMMAND nsc_stage50_10_ownership_examples_documentation_tests)

    add_executable(nsc_stage51_complete_string_tests tests/unit/Stage51CompleteStringTests.cpp)
    target_link_libraries(nsc_stage51_complete_string_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage51_complete_string_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage51_complete_string_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage51_complete_string_tests)
    add_test(NAME nsc.stage51.complete-string COMMAND nsc_stage51_complete_string_tests)

    add_executable(nsc_stage51_enum_oop_support_tests tests/unit/Stage51EnumOopSupportTests.cpp)
    target_link_libraries(nsc_stage51_enum_oop_support_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage51_enum_oop_support_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage51_enum_oop_support_tests)
    add_test(NAME nsc.stage51.enum-oop-support COMMAND nsc_stage51_enum_oop_support_tests)

    add_executable(nsc_stage52_generic_collections_compiler_fixes_tests tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp)
    target_link_libraries(nsc_stage52_generic_collections_compiler_fixes_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_generic_collections_compiler_fixes_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage52_generic_collections_compiler_fixes_tests)
    add_test(NAME nsc.stage52.generic-collections-compiler-fixes COMMAND nsc_stage52_generic_collections_compiler_fixes_tests)

    add_executable(nsc_stage52_1_generic_collections_additions_tests tests/unit/Stage52_1GenericCollectionsAdditionsTests.cpp)
    target_link_libraries(nsc_stage52_1_generic_collections_additions_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_1_generic_collections_additions_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage52_1_generic_collections_additions_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage52_1_generic_collections_additions_tests)
    add_test(NAME nsc.stage52-1.generic-collections-additions COMMAND nsc_stage52_1_generic_collections_additions_tests)

    add_executable(nsc_stage52_2_var_keyword_tests tests/unit/Stage52_2VarKeywordTests.cpp)
    target_link_libraries(nsc_stage52_2_var_keyword_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_2_var_keyword_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage52_2_var_keyword_tests)
    add_test(NAME nsc.stage52-2.var-keyword COMMAND nsc_stage52_2_var_keyword_tests)

    add_executable(nsc_stage52_3_foreach_tests tests/unit/Stage52_3ForeachTests.cpp)
    target_link_libraries(nsc_stage52_3_foreach_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_3_foreach_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage52_3_foreach_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage52_3_foreach_tests)
    add_test(NAME nsc.stage52-3.foreach COMMAND nsc_stage52_3_foreach_tests)

    # Stage 52.4: owned-parameter cleanup correctness, verified through REAL EXECUTION (Wine when
    # available, gracefully skipped otherwise) -- not just structural COFF/PE checks. Closure
    # checklist P0 item 3 (owned int + nested call regression) plus the newly found and fixed
    # owned-reference-into-owned-array-without-move() double-free across the whole collections
    # library. See the file's own header comment for the full story.
    add_executable(nsc_stage52_4_owned_cleanup_runtime_tests tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp)
    target_link_libraries(nsc_stage52_4_owned_cleanup_runtime_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_4_owned_cleanup_runtime_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage52_4_owned_cleanup_runtime_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage52_4_owned_cleanup_runtime_tests)
    add_test(NAME nsc.stage52-4.owned-cleanup-runtime COMMAND nsc_stage52_4_owned_cleanup_runtime_tests)

    # Stage 52.5: the four P0 semantic gaps + P1.1 compatibility gap found by Alberto's dedicated
    # foreach technical review (STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md), sent after both
    # the 52.x foreach feature and the 52.4 owned-cleanup closure round had already been
    # delivered: temporary-name hygiene ($-prefix), an owned-returning collection source, the
    # foreach element's read-only-ness, `const` no longer being silently accepted/discarded, and
    # `Current` becoming a property instead of a method. See the file's own header comment.
    add_executable(nsc_stage52_5_foreach_semantic_gaps_tests tests/unit/Stage52_5ForeachSemanticGapsTests.cpp)
    target_link_libraries(nsc_stage52_5_foreach_semantic_gaps_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_5_foreach_semantic_gaps_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage52_5_foreach_semantic_gaps_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage52_5_foreach_semantic_gaps_tests)
    add_test(NAME nsc.stage52-5.foreach-semantic-gaps COMMAND nsc_stage52_5_foreach_semantic_gaps_tests)

    # Stage 52.6: P1.2 of the same review -- GetEnumerator()/foreach extended to Queue<T>,
    # Stack<T>, LinkedList<T>, HashSet<T>, SortedSet<T>, PriorityQueue<TElement,TPriority>, plus
    # the deliberate Dictionary<TKey,TValue>/SortedDictionary<TKey,TValue>/SortedList<TKey,TValue>
    # non-implementation (documented ABI landmine) and its Keys()/Values() alternative.
    add_executable(nsc_stage52_6_foreach_collections_enumerators_tests tests/unit/Stage52_6ForeachCollectionsEnumeratorsTests.cpp)
    target_link_libraries(nsc_stage52_6_foreach_collections_enumerators_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage52_6_foreach_collections_enumerators_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage52_6_foreach_collections_enumerators_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage52_6_foreach_collections_enumerators_tests)
    add_test(NAME nsc.stage52-6.foreach-collections-enumerators COMMAND nsc_stage52_6_foreach_collections_enumerators_tests)

    # Stage 53.1 "Time" -- NativeSharp.Core.Diagnostics.NativeClock's three raw core operations
    # (QueryPerformanceCounter/QueryPerformanceFrequency/GetSystemTimeAsFileTime), plus
    # TimeSpan/DateTime/Stopwatch built on top of them in ordinary NativeSharp. Verified through
    # REAL EXECUTION (Wine when available, gracefully skipped otherwise), same discipline as
    # Stage 52.4/52.5/52.6: structural COFF/PE checks alone would not have caught, for example, a
    # wrong FILETIME/Ticks epoch-offset sign or a swapped QueryPerformanceCounter/Frequency call.
    add_executable(nsc_stage53_1_time_tests tests/unit/Stage53_1TimeTests.cpp)
    target_link_libraries(nsc_stage53_1_time_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_1_time_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_1_time_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_1_time_tests)
    add_test(NAME nsc.stage53-1.time COMMAND nsc_stage53_1_time_tests)

    # Stage 53.2 "Environment", ronda 1 (TickCount64/ProcessorCount/Exit) -- same discipline:
    # structural checks plus REAL EXECUTION under Wine, where the final runtime check IS
    # Environment.Exit(0) itself (see examples/project_stage53_2/Main.ns).
    add_executable(nsc_stage53_2_environment_tests tests/unit/Stage53_2EnvironmentTests.cpp)
    target_link_libraries(nsc_stage53_2_environment_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_2_environment_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_2_environment_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_2_environment_tests)
    add_test(NAME nsc.stage53-2.environment COMMAND nsc_stage53_2_environment_tests)

    # Stage 53.2b "Environment (variables)" (GetEnvironmentVariable/SetEnvironmentVariable) --
    # same discipline, but this file tests BOTH O0 and O2: this round's real bug (a hand-rolled
    # copy loop getting idiom-recognized into an @llvm.memcpy call at O2, which the linker's
    # ucrtbase.dll import scan did not know about) only manifested at O2, so testing only O0
    # would not have caught it. See tests/unit/Stage53_2bEnvironmentVariablesTests.cpp.
    add_executable(nsc_stage53_2b_environment_variables_tests tests/unit/Stage53_2bEnvironmentVariablesTests.cpp)
    target_link_libraries(nsc_stage53_2b_environment_variables_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_2b_environment_variables_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_2b_environment_variables_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_2b_environment_variables_tests)
    add_test(NAME nsc.stage53-2b.environment-variables COMMAND nsc_stage53_2b_environment_variables_tests)

    # Stage 53.3 "Filesystem/Paths", ronda 1 (Path pure NativeSharp + File.Exists/Delete +
    # Directory.Exists/CreateDirectory) -- same O0+O2 discipline as 53.2b (these four core
    # operations reuse buildNullTerminatedCopy, the exact helper whose loop the O2 memcpy bug
    # came from), plus real filesystem side effects verified from BOTH inside the NativeSharp
    # program (via File.Exists/Directory.Exists) AND independently from the Linux side (via
    # std::filesystem after the Wine run completes). See
    # tests/unit/Stage53_3FilesystemPathsTests.cpp for the full rationale.
    add_executable(nsc_stage53_3_filesystem_paths_tests tests/unit/Stage53_3FilesystemPathsTests.cpp)
    target_link_libraries(nsc_stage53_3_filesystem_paths_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_3_filesystem_paths_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_3_filesystem_paths_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_3_filesystem_paths_tests)
    add_test(NAME nsc.stage53-3.filesystem-paths COMMAND nsc_stage53_3_filesystem_paths_tests)

    # Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- File.ReadAllText/WriteAllText/
    # AppendAllText, the piece Stage 53.3's first round deliberately deferred (CreateFileA +
    # GetFileSizeEx + ReadFile/WriteFile + CloseHandle, a longer Win32 sequence than any single-
    # call operation Stage 53 used before). Same O0+O2 discipline, same dedicated-working-
    # directory pattern Stage 53.3's own test introduced. See
    # tests/unit/Stage53_3bFilesystemPathsContentTests.cpp for the full rationale.
    add_executable(nsc_stage53_3b_filesystem_paths_content_tests tests/unit/Stage53_3bFilesystemPathsContentTests.cpp)
    target_link_libraries(nsc_stage53_3b_filesystem_paths_content_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_3b_filesystem_paths_content_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_3b_filesystem_paths_content_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_3b_filesystem_paths_content_tests)
    add_test(NAME nsc.stage53-3b.filesystem-paths-content COMMAND nsc_stage53_3b_filesystem_paths_content_tests)

    # Stage 53.4 "Filesystem/Paths (cierre)": Directory.Delete (calco de File.Delete) y
    # Path.GetFullPath (primera operacion nucleo de Path, dos llamadas Win32 igual que
    # Environment.GetEnvironmentVariable). Same O0+O2 discipline, same dedicated-working-
    # directory pattern every filesystem-touching Stage 53 round has used since 53.3. See
    # tests/unit/Stage53_4FilesystemPathsCierreTests.cpp for the full rationale.
    add_executable(nsc_stage53_4_filesystem_paths_cierre_tests tests/unit/Stage53_4FilesystemPathsCierreTests.cpp)
    target_link_libraries(nsc_stage53_4_filesystem_paths_cierre_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_4_filesystem_paths_cierre_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_4_filesystem_paths_cierre_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_4_filesystem_paths_cierre_tests)
    add_test(NAME nsc.stage53-4.filesystem-paths-cierre COMMAND nsc_stage53_4_filesystem_paths_cierre_tests)

    # Stage 53.5 "Command-line arguments" -- Environment.GetCommandLineArgCount/GetCommandLineArgs
    # (GetCommandLineW + CommandLineToArgvW from SHELL32.dll + WideCharToMultiByte + LocalFree --
    # the only *W/UTF-16 Win32 calls in this entire runtime). Same O0+O2 discipline as every prior
    # Stage 53 round, and the SAME std::system()-based Wine-invocation helper every prior round
    # used (no raw POSIX process APIs -- this test file, like the whole project, compiles on both
    # this Linux/Wine sandbox and Alberto's Windows/clang-cl toolchain), just with each argument
    # wrapped in POSIX single quotes before being concatenated, so the exhaustive quoting/escaping
    # test matrix reaches Wine byte-exact instead of being mangled by naive shell concatenation.
    # See tests/unit/Stage53_5CommandLineArgsTests.cpp for the full rationale.
    add_executable(nsc_stage53_5_command_line_args_tests tests/unit/Stage53_5CommandLineArgsTests.cpp)
    target_link_libraries(nsc_stage53_5_command_line_args_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_5_command_line_args_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_5_command_line_args_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_5_command_line_args_tests)
    add_test(NAME nsc.stage53-5.command-line-args COMMAND nsc_stage53_5_command_line_args_tests)

    # Stage 53.6 "Enumeracion de directorios" -- Directory.GetFileCount/GetFiles/GetDirectoryCount/
    # GetDirectories (FindFirstFileA/FindNextFileA/FindClose, the first use of these three in this
    # runtime -- see src/Runtime/WindowsRuntimeBuilder.cpp). Arreglo de dos pasadas, un solo par de
    # funciones C++ compartidas parametrizadas por un booleano `wantDirectories` que decide en
    # tiempo de generacion de codigo que filtro de atributo emitir. Same O0+O2 discipline as every
    # prior Stage 53 round, and the same portable std::system()-based Wine-invocation helper (no
    # raw POSIX process APIs). See tests/unit/Stage53_6DirectoryEnumerationTests.cpp for the full
    # rationale. This closes Stage 53.
    add_executable(nsc_stage53_6_directory_enumeration_tests tests/unit/Stage53_6DirectoryEnumerationTests.cpp)
    target_link_libraries(nsc_stage53_6_directory_enumeration_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage53_6_directory_enumeration_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage53_6_directory_enumeration_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage53_6_directory_enumeration_tests)
    add_test(NAME nsc.stage53-6.directory-enumeration COMMAND nsc_stage53_6_directory_enumeration_tests)

    # Stage 54.1 "Threads" -- NativeSharp.Core.Threading.Thread, RAII sobre CreateThread/
    # WaitForSingleObject/CloseHandle, con auto-Join() en el destructor. Primera core operation
    # cuyo handle de Win32 sobrevive entre llamadas NativeSharp separadas. Rigor extra explicito
    # de Alberto para Stage 54 (ver claude/05_metodo_de_trabajo_y_archivo_historico.md): el
    # ejemplo empaquetado corre bajo Wine varias veces por nivel de optimizacion (no una sola vez
    # como cada ronda de Stage 53), exigiendo exit code 0 sin crash en CADA corrida. See
    # tests/unit/Stage54_1ThreadsTests.cpp for the full rationale.
    add_executable(nsc_stage54_1_threads_tests tests/unit/Stage54_1ThreadsTests.cpp)
    target_link_libraries(nsc_stage54_1_threads_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage54_1_threads_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage54_1_threads_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage54_1_threads_tests)
    add_test(NAME nsc.stage54-1.threads COMMAND nsc_stage54_1_threads_tests)

    # Stage 54.2 "Atomics" -- same extra rigor as Stage 54.1 (see tests/unit/Stage54_2AtomicsTests.cpp
    # for the full rationale, including the two design pivots this stage required).
    add_executable(nsc_stage54_2_atomics_tests tests/unit/Stage54_2AtomicsTests.cpp)
    target_link_libraries(nsc_stage54_2_atomics_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage54_2_atomics_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage54_2_atomics_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage54_2_atomics_tests)
    add_test(NAME nsc.stage54-2.atomics COMMAND nsc_stage54_2_atomics_tests)

    # Stage 54.3 "Mutex" -- same extra rigor as Stage 54.1/54.2, PLUS dedicated regression coverage
    # for the two pre-existing, compiler-wide cctor-guard bugs this round found and fixed (a
    # cross-thread TOCTOU race and a same-thread self-deadlock introduced by that race's fix -- see
    # tests/unit/Stage54_3MutexTests.cpp's header comment for the full rationale).
    add_executable(nsc_stage54_3_mutex_tests tests/unit/Stage54_3MutexTests.cpp)
    target_link_libraries(nsc_stage54_3_mutex_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage54_3_mutex_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage54_3_mutex_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage54_3_mutex_tests)
    add_test(NAME nsc.stage54-3.mutex COMMAND nsc_stage54_3_mutex_tests)

    # Stage 54.4 "Condition Variables" -- same extra rigor as Stage 54.1/54.2/54.3, PLUS two
    # dedicated regression fixtures (PulseAll() broadcast-vs-single-wake, and Wait(timeout) genuine
    # expiration) -- see tests/unit/Stage54_4MonitorTests.cpp's header comment for the full rationale.
    add_executable(nsc_stage54_4_monitor_tests tests/unit/Stage54_4MonitorTests.cpp)
    target_link_libraries(nsc_stage54_4_monitor_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage54_4_monitor_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage54_4_monitor_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage54_4_monitor_tests)
    add_test(NAME nsc.stage54-4.monitor COMMAND nsc_stage54_4_monitor_tests)

    # Stage 54.5 "Otras primitivas de sincronizacion" -- same extra rigor, PLUS a dedicated
    # regression fixture for Semaphore's real concurrency limit (see
    # tests/unit/Stage54_5SyncPrimitivesTests.cpp's header comment for the full rationale).
    add_executable(nsc_stage54_5_sync_primitives_tests tests/unit/Stage54_5SyncPrimitivesTests.cpp)
    target_link_libraries(nsc_stage54_5_sync_primitives_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage54_5_sync_primitives_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage54_5_sync_primitives_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage54_5_sync_primitives_tests)
    add_test(NAME nsc.stage54-5.sync-primitives COMMAND nsc_stage54_5_sync_primitives_tests)

    # Stage 55.1 "Process" -- same extra rigor as Stage 53/54 (see
    # tests/unit/Stage55_1ProcessTests.cpp's header comment for the full rationale).
    add_executable(nsc_stage55_1_process_tests tests/unit/Stage55_1ProcessTests.cpp)
    target_link_libraries(nsc_stage55_1_process_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage55_1_process_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage55_1_process_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage55_1_process_tests)
    add_test(NAME nsc.stage55-1.process COMMAND nsc_stage55_1_process_tests)

    # Stage 55.2 "Hardware / System Info" -- same extra rigor as Stage 53/54 (see
    # tests/unit/Stage55_2HardwareTests.cpp's header comment for the full rationale).
    add_executable(nsc_stage55_2_hardware_tests tests/unit/Stage55_2HardwareTests.cpp)
    target_link_libraries(nsc_stage55_2_hardware_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage55_2_hardware_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage55_2_hardware_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage55_2_hardware_tests)
    add_test(NAME nsc.stage55-2.hardware COMMAND nsc_stage55_2_hardware_tests)

    # Stage 55.3 "Diagnostics" -- same extra rigor as Stage 53/54 (see
    # tests/unit/Stage55_3DiagnosticsTests.cpp's header comment for the full rationale).
    add_executable(nsc_stage55_3_diagnostics_tests tests/unit/Stage55_3DiagnosticsTests.cpp)
    target_link_libraries(nsc_stage55_3_diagnostics_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage55_3_diagnostics_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage55_3_diagnostics_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage55_3_diagnostics_tests)
    add_test(NAME nsc.stage55-3.diagnostics COMMAND nsc_stage55_3_diagnostics_tests)

    # Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29) -- same extra rigor as
    # Stage 53/54/55 (see tests/unit/PreStage56UtilitiesTests.cpp's header comment for the full
    # rationale).
    add_executable(nsc_prestage56_utilities_tests tests/unit/PreStage56UtilitiesTests.cpp)
    target_link_libraries(nsc_prestage56_utilities_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_prestage56_utilities_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_prestage56_utilities_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_prestage56_utilities_tests)
    add_test(NAME nsc.prestage56.utilities COMMAND nsc_prestage56_utilities_tests)

    nsc_add_unit_test(nsc_semantic_tests tests/unit/SemanticTests.cpp nsc.semantics)
    nsc_add_unit_test(nsc_hir_tests tests/unit/HirTests.cpp nsc.hir)
    nsc_add_unit_test(nsc_hir_verifier_tests tests/unit/HirVerifierTests.cpp nsc.hir-verifier)
    nsc_add_unit_test(nsc_mir_tests tests/unit/MirTests.cpp nsc.mir)
    nsc_add_unit_test(nsc_mir_verifier_tests tests/unit/MirVerifierTests.cpp nsc.mir-verifier)
    nsc_add_unit_test(nsc_cfg_tests tests/unit/CfgTests.cpp nsc.cfg)
    nsc_add_unit_test(nsc_cfg_verifier_tests tests/unit/CfgVerifierTests.cpp nsc.cfg-verifier)
    nsc_add_unit_test(nsc_dominance_tests tests/unit/DominanceTests.cpp nsc.dominance)
    nsc_add_unit_test(
        nsc_dominance_verifier_tests
        tests/unit/DominanceVerifierTests.cpp
        nsc.dominance-verifier)
    nsc_add_unit_test(nsc_ssa_tests tests/unit/SsaTests.cpp nsc.ssa)
    nsc_add_unit_test(
        nsc_ssa_verifier_tests
        tests/unit/SsaVerifierTests.cpp
        nsc.ssa-verifier)
    nsc_add_unit_test(
        nsc_optimization_passes_tests
        tests/unit/OptimizationPassesTests.cpp
        nsc.optimization-passes)
    nsc_add_unit_test(
        nsc_constant_folder_tests
        tests/unit/ConstantFolderTests.cpp
        nsc.constant-folder)
    nsc_add_unit_test(
        nsc_pass_manager_tests
        tests/unit/PassManagerTests.cpp
        nsc.pass-manager)
    nsc_add_unit_test(
        nsc_optimization_verifier_tests
        tests/unit/OptimizationVerifierTests.cpp
        nsc.optimization-verifier)
    nsc_add_unit_test(
        nsc_frontend_robustness_tests
        tests/unit/FrontendRobustnessTests.cpp
        nsc.frontend-robustness)

    add_executable(nsc_llvm_lowering_tests tests/unit/LlvmLoweringTests.cpp)
    target_link_libraries(nsc_llvm_lowering_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_llvm_lowering_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_llvm_lowering_tests)
    add_test(NAME nsc.llvm-lowering COMMAND nsc_llvm_lowering_tests)

    add_executable(nsc_llvm_codegen_tests tests/unit/LlvmCodeGenerationTests.cpp)
    target_link_libraries(nsc_llvm_codegen_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_llvm_codegen_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_llvm_codegen_tests)
    add_test(NAME nsc.llvm-codegen COMMAND nsc_llvm_codegen_tests)

    add_executable(nsc_llvm_optimization_tests tests/unit/LlvmOptimizationTests.cpp)
    target_link_libraries(nsc_llvm_optimization_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_llvm_optimization_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_llvm_optimization_tests)
    add_test(NAME nsc.llvm-optimization COMMAND nsc_llvm_optimization_tests)

    add_executable(nsc_windows_runtime_tests tests/unit/WindowsRuntimeTests.cpp)
    target_link_libraries(nsc_windows_runtime_tests PRIVATE nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_windows_runtime_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_windows_runtime_tests)
    add_test(NAME nsc.windows-runtime COMMAND nsc_windows_runtime_tests)

    add_executable(nsc_windows_linker_tests tests/unit/WindowsLinkerTests.cpp)
    target_link_libraries(nsc_windows_linker_tests PRIVATE nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_windows_linker_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_windows_linker_tests)
    add_test(NAME nsc.windows-linker COMMAND nsc_windows_linker_tests)

    add_executable(nsc_driver_tests tests/unit/DriverTests.cpp)
    target_link_libraries(nsc_driver_tests PRIVATE nsc_driver nsc_frontend nsc_target)
    target_compile_features(nsc_driver_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_driver_tests)
    add_test(NAME nsc.driver COMMAND nsc_driver_tests)

    # CC1-000: el regex incluye ahora la linea `build <id>`. La stage subio a 57, y este test es
    # justo el que se le habia quedado atras al numero anterior -- por eso se le agrega tambien la
    # comprobacion del build id: si la generacion de BuildId.h se rompiera, esto se pone rojo aqui
    # ademas de en nsc.version.identity.
    #
    # `[0-9a-f]` y no `.*` a proposito: un @NSC_BUILD_ID@ sin sustituir dejaria la linea vacia, y un
    # `.*` la aceptaria igual.
    add_test(NAME nsc.version COMMAND "${NSC_TEST_COMPILER}" --version)
    set_tests_properties(
        nsc.version
        PROPERTIES PASS_REGULAR_EXPRESSION
            "nsc 0.8.0-alpha[+]build[.][0-9TZ]+[.][0-9a-f]+.*NativeSharp 0.1.*stage 57.*build [0-9a-f][0-9a-f]+")

    add_test(
        NAME nsc.stage25.project-names
        COMMAND "${NSC_TEST_COMPILER}" --check-project-names
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage25/App/App.nsproj)
    set_tests_properties(
        nsc.stage25.project-names
        PROPERTIES PASS_REGULAR_EXPRESSION "project name lookup check: ok")

    add_test(
        NAME nsc.stage26.project-members
        COMMAND "${NSC_TEST_COMPILER}" --check-project-members
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage26/App/App.nsproj)
    set_tests_properties(
        nsc.stage26.project-members
        PROPERTIES PASS_REGULAR_EXPRESSION "project member binding check: ok")


    add_test(
        NAME nsc.stage27.project-method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --check-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage27/App/Stage27App.nsproj)
    set_tests_properties(
        nsc.stage27.project-method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "project method-body semantic check: ok")

    add_test(
        NAME nsc.stage28.project-constructions
        COMMAND "${NSC_TEST_COMPILER}" --check-project-constructions
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage28/App/Stage28App.nsproj)
    set_tests_properties(
        nsc.stage28.project-constructions
        PROPERTIES PASS_REGULAR_EXPRESSION "project construction semantic check: ok")

    add_test(
        NAME nsc.stage29.project-layouts
        COMMAND "${NSC_TEST_COMPILER}" --check-project-layouts
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage29/App/Stage29App.nsproj)
    set_tests_properties(
        nsc.stage29.project-layouts
        PROPERTIES PASS_REGULAR_EXPRESSION "project object layout check: ok")

    add_test(
        NAME nsc.stage30.project-oop-llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage30/App/Stage30App.nsproj)
    set_tests_properties(
        nsc.stage30.project-oop-llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE30_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage30_example.obj)
    add_test(
        NAME nsc.stage30.project-oop-object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage30/App/Stage30App.nsproj
            -O2 -o ${NSC_STAGE30_OBJECT})
    set_tests_properties(
        nsc.stage30.project-oop-object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage31.generalized-receivers
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage31/App/Stage31App.nsproj)
    set_tests_properties(
        nsc.stage31.generalized-receivers
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    add_test(
        NAME nsc.stage31.receiver-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage31/App/Stage31App.nsproj)
    set_tests_properties(
        nsc.stage31.receiver-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "receiver=explicit-object")

    add_test(
        NAME nsc.stage32.inheritance-graph
        COMMAND "${NSC_TEST_COMPILER}" --check-project-inheritance
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage32/App/Stage32App.nsproj)
    set_tests_properties(
        nsc.stage32.inheritance-graph
        PROPERTIES PASS_REGULAR_EXPRESSION "project inheritance check: ok.*4 classes.*3 base edges")

    add_test(
        NAME nsc.stage33.constructor-chaining
        COMMAND "${NSC_TEST_COMPILER}" --check-project-constructions
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage33/App/Stage33App.nsproj)
    set_tests_properties(
        nsc.stage33.constructor-chaining
        PROPERTIES PASS_REGULAR_EXPRESSION "project construction semantic check: ok")

    add_test(
        NAME nsc.stage33.inheritance-layout
        COMMAND "${NSC_TEST_COMPILER}" --check-project-layouts
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage33/App/Stage33App.nsproj)
    set_tests_properties(
        nsc.stage33.inheritance-layout
        PROPERTIES PASS_REGULAR_EXPRESSION "project object layout check: ok")

    add_test(
        NAME nsc.stage33.inheritance-llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage33/App/Stage33App.nsproj)
    set_tests_properties(
        nsc.stage33.inheritance-llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    add_test(
        NAME nsc.stage34.virtual-dispatch
        COMMAND "${NSC_TEST_COMPILER}" --check-project-virtual-dispatch
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage34/App/Stage34App.nsproj)
    set_tests_properties(
        nsc.stage34.virtual-dispatch
        PROPERTIES PASS_REGULAR_EXPRESSION "project virtual dispatch check: ok.*2 virtual methods")

    add_test(
        NAME nsc.stage34.virtual-llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage34/App/Stage34App.nsproj)
    set_tests_properties(
        nsc.stage34.virtual-llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE34_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage34_virtual_example.obj)
    add_test(
        NAME nsc.stage34.virtual-object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage34/App/Stage34App.nsproj
            -O2 -o ${NSC_STAGE34_OBJECT})
    set_tests_properties(
        nsc.stage34.virtual-object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage37.method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --check-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage37/App/Stage37App.nsproj)
    set_tests_properties(
        nsc.stage37.method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "project method-body semantic check: ok")

    add_test(
        NAME nsc.stage37.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage37/App/Stage37App.nsproj)
    set_tests_properties(
        nsc.stage37.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "abi=.*static,p0:builtin:int")

    add_test(
        NAME nsc.stage37.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage37/App/Stage37App.nsproj)
    set_tests_properties(
        nsc.stage37.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE37_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage37_static_example.obj)
    add_test(
        NAME nsc.stage37.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage37/App/Stage37App.nsproj
            -O2 -o ${NSC_STAGE37_OBJECT})
    set_tests_properties(
        nsc.stage37.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage35-recovery.destruction
        COMMAND "${NSC_TEST_COMPILER}" --check-project-destruction
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage35_recovery/App/Stage35RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage35-recovery.destruction
        PROPERTIES PASS_REGULAR_EXPRESSION "project destruction semantic check: ok")

    add_test(
        NAME nsc.stage35-recovery.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage35_recovery/App/Stage35RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage35-recovery.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "destruction=dynamic-descriptor")

    add_test(
        NAME nsc.stage35-recovery.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage35_recovery/App/Stage35RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage35-recovery.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE35_RECOVERY_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage35_recovery_example.obj)
    add_test(
        NAME nsc.stage35-recovery.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage35_recovery/App/Stage35RecoveryApp.nsproj
            -O2 -o ${NSC_STAGE35_RECOVERY_OBJECT})
    set_tests_properties(
        nsc.stage35-recovery.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    # NSC-LIMIT-007. La entrada de driver de la fase de constraints, ejercitada de verdad.
    #
    # Una fase sin prueba de ctest solo existe para quien lee el codigo: la revision encontro que
    # `--check-project-generic-constraints` no la tenia, y de paso que la bandera no era una fase
    # sino un alias de la de cuerpos. Las dos cosas se arreglaron; esto es lo que sostiene la
    # segunda, porque la fixture c44 tiene cuerpos --y si la bandera volviera a arrastrarlos, un
    # fallo de cuerpos se veria aqui como un fallo de constraints.
    add_test(
        NAME nsc.cc1.generic-constraints
        COMMAND "${NSC_TEST_COMPILER}" --check-project-generic-constraints
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cc1/c44_constraints_genericos/c44_constraints_genericos.nsproj)
    set_tests_properties(
        nsc.cc1.generic-constraints
        PROPERTIES PASS_REGULAR_EXPRESSION "project generic constraint check: ok \\(0 violations\\)")

    add_test(
        NAME nsc.stage36-recovery.interfaces
        COMMAND "${NSC_TEST_COMPILER}" --check-project-interfaces
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage36_recovery/App/Stage36RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage36-recovery.interfaces
        PROPERTIES PASS_REGULAR_EXPRESSION "project interface semantic check: ok")

    add_test(
        NAME nsc.stage36-recovery.interface-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-interfaces
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage36_recovery/App/Stage36RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage36-recovery.interface-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "interface type:Stage36RecoveryApp::Stage36.Demo.IAdvanced")

    add_test(
        NAME nsc.stage36-recovery.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage36_recovery/App/Stage36RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage36-recovery.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "interface-instance-call")

    add_test(
        NAME nsc.stage36-recovery.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage36_recovery/App/Stage36RecoveryApp.nsproj)
    set_tests_properties(
        nsc.stage36-recovery.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE36_RECOVERY_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage36_recovery_example.obj)
    add_test(
        NAME nsc.stage36-recovery.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage36_recovery/App/Stage36RecoveryApp.nsproj
            -O2 -o ${NSC_STAGE36_RECOVERY_OBJECT})
    set_tests_properties(
        nsc.stage36-recovery.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage38.method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --check-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage38/App/Stage38App.nsproj)
    set_tests_properties(
        nsc.stage38.method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "project method-body semantic check: ok")

    add_test(
        NAME nsc.stage38.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage38/App/Stage38App.nsproj)
    set_tests_properties(
        nsc.stage38.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "op_Addition")

    add_test(
        NAME nsc.stage38.interface-dispatch
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage38/App/Stage38App.nsproj)
    set_tests_properties(
        nsc.stage38.interface-dispatch
        PROPERTIES PASS_REGULAR_EXPRESSION "interface-instance-call")

    add_test(
        NAME nsc.stage38.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage38/App/Stage38App.nsproj)
    set_tests_properties(
        nsc.stage38.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE38_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage38_example.obj)
    add_test(
        NAME nsc.stage38.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage38/App/Stage38App.nsproj
            -O2 -o ${NSC_STAGE38_OBJECT})
    set_tests_properties(
        nsc.stage38.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage39.method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage39/App/Stage39App.nsproj)
    set_tests_properties(
        nsc.stage39.method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "runtime-type-bindings=")

    add_test(
        NAME nsc.stage39.hir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-hir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage39/App/Stage39App.nsproj)
    set_tests_properties(
        nsc.stage39.hir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "runtime-type-test")

    add_test(
        NAME nsc.stage39.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage39/App/Stage39App.nsproj)
    set_tests_properties(
        nsc.stage39.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "checked-reference-cast")

    add_test(
        NAME nsc.stage39.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage39/App/Stage39App.nsproj)
    set_tests_properties(
        nsc.stage39.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE39_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage39_example.obj)
    add_test(
        NAME nsc.stage39.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage39/App/Stage39App.nsproj
            -O2 -o ${NSC_STAGE39_OBJECT})
    set_tests_properties(
        nsc.stage39.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage40.members
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-members
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj)
    set_tests_properties(
        nsc.stage40.members
        PROPERTIES PASS_REGULAR_EXPRESSION "kind=struct")

    add_test(
        NAME nsc.stage40.layouts
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-layouts
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj)
    set_tests_properties(
        nsc.stage40.layouts
        PROPERTIES PASS_REGULAR_EXPRESSION "struct-values=3")

    add_test(
        NAME nsc.stage40.hir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-hir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj)
    set_tests_properties(
        nsc.stage40.hir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "local-address")

    add_test(
        NAME nsc.stage40.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj)
    set_tests_properties(
        nsc.stage40.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "win64-direct-integer-carrier")

    add_test(
        NAME nsc.stage40.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj)
    set_tests_properties(
        nsc.stage40.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE40_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage40_example.obj)
    add_test(
        NAME nsc.stage40.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage40/App/Stage40App.nsproj
            -O2 -o ${NSC_STAGE40_OBJECT})
    set_tests_properties(
        nsc.stage40.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage42.method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage42/App/Stage42App.nsproj)
    set_tests_properties(
        nsc.stage42.method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "delegate-bindings=6.*event-bindings=3")

    add_test(
        NAME nsc.stage42.hir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-hir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage42/App/Stage42App.nsproj)
    set_tests_properties(
        nsc.stage42.hir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "delegate-create.*event-invoke")

    add_test(
        NAME nsc.stage42.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage42/App/Stage42App.nsproj)
    set_tests_properties(
        nsc.stage42.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "delegate-invoke.*event-remove")

    add_test(
        NAME nsc.stage42.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage42/App/Stage42App.nsproj)
    set_tests_properties(
        nsc.stage42.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE42_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage42_example.obj)
    add_test(
        NAME nsc.stage42.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage42/App/Stage42App.nsproj
            -O2 -o ${NSC_STAGE42_OBJECT})
    set_tests_properties(
        nsc.stage42.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage43.method-bodies
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-method-bodies
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage43/App/Stage43App.nsproj)
    set_tests_properties(
        nsc.stage43.method-bodies
        PROPERTIES PASS_REGULAR_EXPRESSION "lambda-bindings=10.*captures=2")

    add_test(
        NAME nsc.stage43.hir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-hir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage43/App/Stage43App.nsproj)
    set_tests_properties(
        nsc.stage43.hir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "lambda-create.*closure-capture-value")

    add_test(
        NAME nsc.stage43.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage43/App/Stage43App.nsproj)
    set_tests_properties(
        nsc.stage43.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "lambda-body captures=2.*closure-capture-load")

    add_test(
        NAME nsc.stage43.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage43/App/Stage43App.nsproj)
    set_tests_properties(
        nsc.stage43.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE43_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage43_example.obj)
    add_test(
        NAME nsc.stage43.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage43/App/Stage43App.nsproj
            -O2 -o ${NSC_STAGE43_OBJECT})
    set_tests_properties(
        nsc.stage43.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(
        NAME nsc.stage44.generics
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-generics
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage44/App/Stage44App.nsproj)
    set_tests_properties(
        nsc.stage44.generics
        PROPERTIES PASS_REGULAR_EXPRESSION "templates=5 specializations=7.*Identity__g1__long")

    add_test(
        NAME nsc.stage44.hir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-hir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage44/App/Stage44App.nsproj)
    set_tests_properties(
        nsc.stage44.hir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "generic-specialization=.*Identity``1<int>")

    add_test(
        NAME nsc.stage44.mir-dump
        COMMAND "${NSC_TEST_COMPILER}" --dump-project-oop-mir
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage44/App/Stage44App.nsproj)
    set_tests_properties(
        nsc.stage44.mir-dump
        PROPERTIES PASS_REGULAR_EXPRESSION "generic-specialization=.*Identity``1<long>")

    add_test(
        NAME nsc.stage44.llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-project-oop-llvm
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage44/App/Stage44App.nsproj)
    set_tests_properties(
        nsc.stage44.llvm
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP LLVM verification: ok")

    set(NSC_STAGE44_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/stage44_example.obj)
    add_test(
        NAME nsc.stage44.object
        COMMAND "${NSC_TEST_COMPILER}" --emit-project-oop-obj
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/project_stage44/App/Stage44App.nsproj
            -O2 -o ${NSC_STAGE44_OBJECT})
    set_tests_properties(
        nsc.stage44.object
        PROPERTIES PASS_REGULAR_EXPRESSION "project OOP object emission: ok.*machine=0x8664")

    add_test(NAME nsc.target COMMAND "${NSC_TEST_COMPILER}" --print-target)
    add_test(NAME nsc.codegen-target COMMAND "${NSC_TEST_COMPILER}" --print-codegen)
    set_tests_properties(nsc.codegen-target PROPERTIES PASS_REGULAR_EXPRESSION "0x8664")

    add_test(NAME nsc.llvm-pipeline-o2 COMMAND "${NSC_TEST_COMPILER}" --print-llvm-pipeline -O2)
    set_tests_properties(nsc.llvm-pipeline-o2 PROPERTIES PASS_REGULAR_EXPRESSION "default<O2>")
    set_tests_properties(nsc.target PROPERTIES PASS_REGULAR_EXPRESSION "x86_64-pc-windows-msvc")

    add_test(NAME nsc.runtime-dump COMMAND "${NSC_TEST_COMPILER}" --dump-runtime-llvm)
    set_tests_properties(nsc.runtime-dump PROPERTIES PASS_REGULAR_EXPRESSION "define void @__nativesharp_start")

    add_test(NAME nsc.linker-info COMMAND "${NSC_TEST_COMPILER}" --print-linker)
    set_tests_properties(nsc.linker-info PROPERTIES PASS_REGULAR_EXPRESSION "lld-link")

    set(NSC_INTEGRATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration)

    add_test(NAME nsc.integration.full COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    add_test(NAME nsc.integration.control-flow COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/control_flow.ns)
    add_test(NAME nsc.integration.memory-types COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/memory_types.ns)
    add_test(NAME nsc.integration.hir-features COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/hir_features.ns)
    add_test(NAME nsc.integration.mir-features COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/mir_features.ns)
    add_test(NAME nsc.integration.cfg-features COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/cfg_features.ns)
    add_test(
        NAME nsc.integration.dominance-features
        COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/dominance_features.ns)
    add_test(
        NAME nsc.integration.ssa-features
        COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/ssa_features.ns)
    add_test(
        NAME nsc.integration.pass-features
        COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/pass_features.ns)
    add_test(NAME nsc.integration.library-check COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/library_unit.ns)

    add_test(NAME nsc.invalid.program-missing-main COMMAND "${NSC_TEST_COMPILER}" ${NSC_INTEGRATION_DIR}/valid/library_unit.ns)
    set_tests_properties(nsc.invalid.program-missing-main PROPERTIES WILL_FAIL TRUE)

    add_test(NAME nsc.dump-tokens COMMAND "${NSC_TEST_COMPILER}" --dump-tokens ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.dump-tokens PROPERTIES PASS_REGULAR_EXPRESSION "KwExtern.*KwC")

    add_test(NAME nsc.dump-ast COMMAND "${NSC_TEST_COMPILER}" --dump-ast ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.dump-ast PROPERTIES PASS_REGULAR_EXPRESSION "CompilationUnit")

    add_test(NAME nsc.dump-symbols COMMAND "${NSC_TEST_COMPILER}" --dump-symbols ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.dump-symbols PROPERTIES PASS_REGULAR_EXPRESSION "Function Main.*defined")

    add_test(NAME nsc.dump-sema COMMAND "${NSC_TEST_COMPILER}" --dump-sema ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(
        nsc.dump-sema
        PROPERTIES PASS_REGULAR_EXPRESSION "conversion implicit-string-literal-to-char-pointer")

    add_test(NAME nsc.dump-hir COMMAND "${NSC_TEST_COMPILER}" --dump-hir ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.dump-hir PROPERTIES PASS_REGULAR_EXPRESSION "FunctionDeclaration symbol=Function:Main")

    add_test(NAME nsc.dump-hir-features COMMAND "${NSC_TEST_COMPILER}" --dump-hir ${NSC_INTEGRATION_DIR}/valid/hir_features.ns)
    set_tests_properties(
        nsc.dump-hir-features
        PROPERTIES PASS_REGULAR_EXPRESSION "CompoundAssignment.*read-conversion=integral-promotion")

    add_test(NAME nsc.verify-hir COMMAND "${NSC_TEST_COMPILER}" --verify-hir ${NSC_INTEGRATION_DIR}/valid/hir_features.ns)
    set_tests_properties(nsc.verify-hir PROPERTIES PASS_REGULAR_EXPRESSION "HIR verification: ok")

    add_test(NAME nsc.dump-mir COMMAND "${NSC_TEST_COMPILER}" --dump-mir ${NSC_INTEGRATION_DIR}/valid/hir_features.ns)
    set_tests_properties(nsc.dump-mir PROPERTIES PASS_REGULAR_EXPRESSION "mir.module")

    add_test(NAME nsc.verify-mir COMMAND "${NSC_TEST_COMPILER}" --verify-mir ${NSC_INTEGRATION_DIR}/valid/hir_features.ns)
    set_tests_properties(nsc.verify-mir PROPERTIES PASS_REGULAR_EXPRESSION "MIR verification: ok")

    add_test(NAME nsc.dump-mir-features COMMAND "${NSC_TEST_COMPILER}" --dump-mir ${NSC_INTEGRATION_DIR}/valid/mir_features.ns)
    set_tests_properties(nsc.dump-mir-features PROPERTIES PASS_REGULAR_EXPRESSION "bounds.check")

    add_test(NAME nsc.verify-mir-features COMMAND "${NSC_TEST_COMPILER}" --verify-mir ${NSC_INTEGRATION_DIR}/valid/mir_features.ns)
    set_tests_properties(nsc.verify-mir-features PROPERTIES PASS_REGULAR_EXPRESSION "MIR verification: ok")

    add_test(NAME nsc.dump-cfg COMMAND "${NSC_TEST_COMPILER}" --dump-cfg ${NSC_INTEGRATION_DIR}/valid/control_flow.ns)
    set_tests_properties(nsc.dump-cfg PROPERTIES PASS_REGULAR_EXPRESSION "cfg.module")

    add_test(NAME nsc.verify-cfg COMMAND "${NSC_TEST_COMPILER}" --verify-cfg ${NSC_INTEGRATION_DIR}/valid/control_flow.ns)
    set_tests_properties(nsc.verify-cfg PROPERTIES PASS_REGULAR_EXPRESSION "CFG verification: ok")

    add_test(NAME nsc.dump-cfg-features COMMAND "${NSC_TEST_COMPILER}" --dump-cfg ${NSC_INTEGRATION_DIR}/valid/cfg_features.ns)
    set_tests_properties(nsc.dump-cfg-features PROPERTIES PASS_REGULAR_EXPRESSION "\[unreachable\]")

    add_test(NAME nsc.verify-cfg-features COMMAND "${NSC_TEST_COMPILER}" --verify-cfg ${NSC_INTEGRATION_DIR}/valid/cfg_features.ns)
    set_tests_properties(nsc.verify-cfg-features PROPERTIES PASS_REGULAR_EXPRESSION "CFG verification: ok")

    add_test(
        NAME nsc.dump-dominance
        COMMAND "${NSC_TEST_COMPILER}" --dump-dominance ${NSC_INTEGRATION_DIR}/valid/dominance_features.ns)
    set_tests_properties(nsc.dump-dominance PROPERTIES PASS_REGULAR_EXPRESSION "dominance.module")

    add_test(
        NAME nsc.verify-dominance
        COMMAND "${NSC_TEST_COMPILER}" --verify-dominance ${NSC_INTEGRATION_DIR}/valid/dominance_features.ns)
    set_tests_properties(
        nsc.verify-dominance
        PROPERTIES PASS_REGULAR_EXPRESSION "Dominance verification: ok")

    add_test(
        NAME nsc.dump-ssa
        COMMAND "${NSC_TEST_COMPILER}" --dump-ssa ${NSC_INTEGRATION_DIR}/valid/ssa_features.ns)
    set_tests_properties(nsc.dump-ssa PROPERTIES PASS_REGULAR_EXPRESSION "phi @value")

    add_test(
        NAME nsc.verify-ssa
        COMMAND "${NSC_TEST_COMPILER}" --verify-ssa ${NSC_INTEGRATION_DIR}/valid/ssa_features.ns)
    set_tests_properties(nsc.verify-ssa PROPERTIES PASS_REGULAR_EXPRESSION "SSA verification: ok")

    add_test(
        NAME nsc.dump-opt
        COMMAND "${NSC_TEST_COMPILER}" --dump-opt ${NSC_INTEGRATION_DIR}/valid/pass_features.ns)
    set_tests_properties(nsc.dump-opt PROPERTIES PASS_REGULAR_EXPRESSION "optimized.ssa.module")

    add_test(
        NAME nsc.verify-opt
        COMMAND "${NSC_TEST_COMPILER}" --verify-opt ${NSC_INTEGRATION_DIR}/valid/pass_features.ns)
    set_tests_properties(nsc.verify-opt PROPERTIES PASS_REGULAR_EXPRESSION "Optimization verification: ok")

    add_test(
        NAME nsc.dump-pass-pipeline
        COMMAND "${NSC_TEST_COMPILER}" --dump-pass-pipeline ${NSC_INTEGRATION_DIR}/valid/pass_features.ns)
    set_tests_properties(nsc.dump-pass-pipeline PROPERTIES PASS_REGULAR_EXPRESSION "pass.pipeline")

    add_test(
        NAME nsc.dump-llvm
        COMMAND "${NSC_TEST_COMPILER}" --dump-llvm ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.dump-llvm PROPERTIES PASS_REGULAR_EXPRESSION "target triple = \"x86_64-pc-windows-msvc\"")

    add_test(
        NAME nsc.verify-llvm
        COMMAND "${NSC_TEST_COMPILER}" --verify-llvm ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns)
    set_tests_properties(nsc.verify-llvm PROPERTIES PASS_REGULAR_EXPRESSION "LLVM verification: ok")

    add_test(
        NAME nsc.dump-llvm-opt-o2
        COMMAND "${NSC_TEST_COMPILER}" --dump-llvm-opt ${NSC_INTEGRATION_DIR}/valid/llvm_optimization_features.ns -O2)
    set_tests_properties(nsc.dump-llvm-opt-o2 PROPERTIES PASS_REGULAR_EXPRESSION "ret i32 40")

    add_test(
        NAME nsc.dump-llvm-raw-with-o2
        COMMAND "${NSC_TEST_COMPILER}" --dump-llvm ${NSC_INTEGRATION_DIR}/valid/llvm_optimization_features.ns -O2)
    set_tests_properties(
        nsc.dump-llvm-raw-with-o2
        PROPERTIES PASS_REGULAR_EXPRESSION "call i32 @Square")

    add_test(
        NAME nsc.verify-llvm-opt-o3
        COMMAND "${NSC_TEST_COMPILER}" --verify-llvm-opt ${NSC_INTEGRATION_DIR}/valid/llvm_features.ns -O3)
    set_tests_properties(
        nsc.verify-llvm-opt-o3
        PROPERTIES PASS_REGULAR_EXPRESSION "LLVM optimization verification: ok")

    add_test(
        NAME nsc.dump-llvm-phi
        COMMAND "${NSC_TEST_COMPILER}" --dump-llvm ${NSC_INTEGRATION_DIR}/valid/ssa_features.ns)
    set_tests_properties(nsc.dump-llvm-phi PROPERTIES PASS_REGULAR_EXPRESSION "phi i32")

    add_test(
        NAME nsc.verify-llvm-features
        COMMAND "${NSC_TEST_COMPILER}" --verify-llvm ${NSC_INTEGRATION_DIR}/valid/llvm_features.ns)
    set_tests_properties(nsc.verify-llvm-features PROPERTIES PASS_REGULAR_EXPRESSION "LLVM verification: ok")

    add_test(
        NAME nsc.dump-llvm-traps
        COMMAND "${NSC_TEST_COMPILER}" --dump-llvm ${NSC_INTEGRATION_DIR}/valid/llvm_features.ns)
    set_tests_properties(nsc.dump-llvm-traps PROPERTIES PASS_REGULAR_EXPRESSION "__nativesharp_trap")

    add_test(
        NAME nsc.emit-exe
        COMMAND "${NSC_TEST_COMPILER}" --emit-exe ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns
            -o ${CMAKE_CURRENT_BINARY_DIR}/nativesharp_stage19_integration.exe)
    set_tests_properties(nsc.emit-exe PROPERTIES PASS_REGULAR_EXPRESSION "PE executable emission: ok")

    add_test(
        NAME nsc.emit-exe-o2
        COMMAND "${NSC_TEST_COMPILER}" --emit-exe ${NSC_INTEGRATION_DIR}/valid/llvm_optimization_features.ns -O2
            -o ${CMAKE_CURRENT_BINARY_DIR}/nativesharp_stage20_o2.exe)
    set_tests_properties(nsc.emit-exe-o2 PROPERTIES PASS_REGULAR_EXPRESSION "optimization: O2")

    add_test(
        NAME nsc.invalid.optimization-level
        COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/valid/full_frontend.ns -O4)
    set_tests_properties(nsc.invalid.optimization-level PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME nsc.invalid.unresolved-link
        COMMAND "${NSC_TEST_COMPILER}" --emit-exe ${NSC_INTEGRATION_DIR}/invalid/linker_unresolved_symbol.ns
            -o ${CMAKE_CURRENT_BINARY_DIR}/nativesharp_stage19_unresolved.exe)
    set_tests_properties(nsc.invalid.unresolved-link PROPERTIES WILL_FAIL TRUE)

    add_test(NAME nsc.invalid.unknown COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/invalid/unknown_identifier.ns)
    set_tests_properties(nsc.invalid.unknown PROPERTIES WILL_FAIL TRUE)

    add_test(NAME nsc.invalid.bounds COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/invalid/bounds.ns)
    set_tests_properties(nsc.invalid.bounds PROPERTIES WILL_FAIL TRUE)

    add_test(NAME nsc.invalid.bool-condition COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/invalid/bool_condition.ns)
    set_tests_properties(nsc.invalid.bool-condition PROPERTIES WILL_FAIL TRUE)

    add_test(NAME nsc.invalid.missing-return COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/invalid/missing_return.ns)
    set_tests_properties(nsc.invalid.missing-return PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME nsc.invalid.llvm-reserved-runtime
        COMMAND "${NSC_TEST_COMPILER}" --check ${NSC_INTEGRATION_DIR}/invalid/llvm_reserved_runtime.ns)
    set_tests_properties(nsc.invalid.llvm-reserved-runtime PROPERTIES WILL_FAIL TRUE)

    # =================================================================================
    # `pruebas` -- UN OBJETIVO CON TODOS LOS EJECUTABLES DE PRUEBA Y NINGUNO DEL COMPILADOR.
    #
    # POR QUE EXISTE. `cmake --build` sin `--target` construye TODO, y eso incluye `nsc`.
    # Para correr la bateria contra un compilador YA instalado --que es lo que hace
    # test_nsc.bat-- reconstruir `nsc` es trabajo de mas y, peor, arriesga cambiar lo que se
    # esta midiendo. `build_nsc.bat` genera el compilador aparte y a proposito; este objetivo
    # es la otra mitad de esa separacion.
    #
    # LA LISTA NO SE ESCRIBE A MANO. Son mas de cien ejecutables y crecen cada semana: una
    # lista escrita aqui envejeceria en silencio y un test nuevo dejaria de construirse sin
    # que nadie lo notara -- la misma forma de defecto que el guardia de fixtures existe para
    # cazar. Se toma del propio sistema de construccion y se filtra por tipo.
    get_property(_nsc_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    set(_nsc_pruebas "")
    foreach(_nsc_t IN LISTS _nsc_targets)
        get_target_property(_nsc_tipo ${_nsc_t} TYPE)
        if(_nsc_tipo STREQUAL "EXECUTABLE" AND NOT _nsc_t STREQUAL "nsc")
            list(APPEND _nsc_pruebas ${_nsc_t})
        endif()
    endforeach()
    list(LENGTH _nsc_pruebas _nsc_pruebas_n)
    if(_nsc_pruebas_n LESS 50)
        # Si el filtro deja de reconocerlos, esto NO puede quedarse en silencio construyendo
        # tres: seria una corrida verde que mide una fraccion. Se para el configure.
        message(FATAL_ERROR
            "el objetivo `pruebas` solo reconocio ${_nsc_pruebas_n} ejecutables; el filtro "
            "de BUILDSYSTEM_TARGETS cambio y dejaria de construir la mayoria de los tests")
    endif()
    add_custom_target(pruebas DEPENDS ${_nsc_pruebas})
    message(STATUS "objetivo `pruebas`: ${_nsc_pruebas_n} ejecutables, sin `nsc`")
endif()
