cmake_minimum_required(VERSION 3.25)

project(
    NativeSharp
    VERSION 2.0.0
    DESCRIPTION "NativeSharp 2.0 stable compiler"
    LANGUAGES CXX)

option(NSC_BUILD_TESTS "Build NativeSharp frontend tests" ON)
option(NSC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(NSC_BUILD_ID_OVERRIDE "" CACHE STRING
    "Fixed 16-digit lowercase hexadecimal build id; reserved for reproducibility gates")
if(NOT NSC_BUILD_ID_OVERRIDE STREQUAL "")
    string(LENGTH "${NSC_BUILD_ID_OVERRIDE}" NSC_BUILD_ID_OVERRIDE_LENGTH)
    if(NOT NSC_BUILD_ID_OVERRIDE_LENGTH EQUAL 16 OR
       NOT NSC_BUILD_ID_OVERRIDE MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "NSC_BUILD_ID_OVERRIDE must contain exactly 16 lowercase hexadecimal digits")
    endif()
endif()
set(NSC_BUILD_ID_OVERRIDE_ARGUMENT)
if(NOT NSC_BUILD_ID_OVERRIDE STREQUAL "")
    set(NSC_BUILD_ID_OVERRIDE_ARGUMENT
        "-DNSC_BUILD_ID_OVERRIDE=${NSC_BUILD_ID_OVERRIDE}")
endif()

# CC1-000 / version de build. La version compatible es estable; la identidad visible cambia en
# CADA construccion real de nsc, incluso en un build incremental que no vuelve a configurar CMake.
# `nsc_build_identity` es un target siempre desactualizado y nsc_driver depende de el. Solo
# Version.cpp incluye el header generado: asi no existe una carrera con los muchos tests que
# incluyen Version.h y tampoco se obliga a recompilar todo el frontend por cambiar un string.
set(NSC_COMPILER_RELEASE_VERSION "${PROJECT_VERSION}")
set(NSC_COMPILER_VERSION_BASE "${NSC_COMPILER_RELEASE_VERSION}")
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
        "-DNSC_VERSION_BASE=${NSC_COMPILER_VERSION_BASE}"
        ${NSC_BUILD_ID_OVERRIDE_ARGUMENT}
        -P "${NSC_BUILD_ID_GENERATOR}"
    RESULT_VARIABLE NSC_INITIAL_BUILD_ID_RESULT)
if(NOT NSC_INITIAL_BUILD_ID_RESULT EQUAL 0)
    message(FATAL_ERROR "No se pudo generar la identidad inicial de nsc")
endif()

add_custom_target(nsc_build_identity
    COMMAND "${CMAKE_COMMAND}"
        "-DNSC_OUTPUT=${NSC_GENERATED_BUILD_ID_HEADER}"
        "-DNSC_TEMPLATE=${NSC_BUILD_ID_TEMPLATE}"
        "-DNSC_VERSION_BASE=${NSC_COMPILER_VERSION_BASE}"
        ${NSC_BUILD_ID_OVERRIDE_ARGUMENT}
        -P "${NSC_BUILD_ID_GENERATOR}"
    BYPRODUCTS "${NSC_GENERATED_BUILD_ID_HEADER}"
    COMMENT "Generando version unica para esta construccion de nsc"
    VERBATIM)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")
message(STATUS
    "NativeSharp compiler line: ${NSC_COMPILER_RELEASE_VERSION}; "
    "stable identity: ${NSC_COMPILER_VERSION_BASE}+build.<timestamp>.<id>")

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
    src/Driver/CompilationJobPolicy.cpp
    src/Driver/CompilationWorkerPool.cpp
    src/Compilation/SimpleXmlParser.cpp
    src/Compilation/SdkManifest.cpp
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
    src/Basic/Sha256.cpp
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
    src/Frontend/StageBoundaryVerifier.cpp
    src/Frontend/ComptimeEngine.cpp
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
    if(LLVM_PACKAGE_VERSION VERSION_LESS 23 OR LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL 24)
        message(FATAL_ERROR
            "NativeSharp 2.0 supports LLVM 23.x; the API floor is LLVM 20 because "
            "LLVMRunPassesOnFunction is required for -O0 SEH legalization, but only the LLVM 23 "
            "line is qualified for the 1.0 release contract; "
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
    src/LLVM/CompilationArtifactCache.cpp
    src/LLVM/DebugInformationKind.cpp
    src/LLVM/FloatingPointMode.cpp
    src/LLVM/LlvmModule.cpp
    src/LLVM/LinkTimeOptimizationMode.cpp
    src/LLVM/LlvmNativeSanitizer.cpp
    src/LLVM/LlvmVersion.cpp
    src/LLVM/LlvmOptimizationLevel.cpp
    src/LLVM/LlvmOptimizationObservation.cpp
    src/LLVM/LlvmProcessTrace.cpp
    src/LLVM/LlvmOptimizer.cpp
    src/LLVM/LlvmLowerer.cpp
    src/LLVM/ProjectOopCodePartition.cpp
    src/LLVM/ProjectOopObjectGenerator.cpp
    src/LLVM/ProjectOopLlvmLowerer.cpp
    src/LLVM/LlvmTargetMachine.cpp
    src/LLVM/CoffObjectVerifier.cpp)

target_include_directories(nsc_llvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_llvm PUBLIC nsc_frontend nsc_target PRIVATE ${NSC_LLVM_LINK_LIBRARIES})
target_compile_features(nsc_llvm PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_llvm)

add_library(nsc_runtime STATIC
    src/Runtime/RuntimeAbiContract.cpp
    src/Runtime/WindowsRuntimeBuilder.cpp)

target_include_directories(nsc_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_runtime PUBLIC nsc_llvm)
target_compile_features(nsc_runtime PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_runtime)

add_library(nsc_linker STATIC
    src/Linker/LldLinker.cpp
    src/Linker/CoffArchiveVerifier.cpp
    src/Linker/PeExecutableVerifier.cpp
    src/Linker/PdbVerifier.cpp
    src/Linker/WindowsExecutableBuilder.cpp
    src/Linker/WindowsLibraryBuilder.cpp
    src/Linker/WindowsRuntimeLibrary.cpp
    src/Linker/WindowsRuntimeImports.cpp)

target_include_directories(nsc_linker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsc_linker PUBLIC nsc_runtime nsc_llvm)
target_compile_definitions(nsc_linker PRIVATE
    NSC_DEFAULT_WINDOWS_RUNTIME_LIBRARY="${CMAKE_INSTALL_PREFIX}/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
target_compile_features(nsc_linker PUBLIC cxx_std_20)
nsc_enable_warnings(nsc_linker)

add_library(nsc_driver STATIC
    src/Basic/Version.cpp
    src/Driver/BreakingChangeLedger.cpp
    src/Driver/CompilationProfile.cpp
    src/Driver/CompilationGraphScheduler.cpp
    src/Driver/CompatibilityPolicy.cpp
    src/Driver/DiagnosticBundle.cpp
    src/Driver/FinalBaselineContract.cpp
    src/Driver/Driver.cpp
    src/Driver/InstallationManifest.cpp
    src/Driver/InstallationVerifier.cpp
    src/Driver/MigrationBaseline.cpp
    src/Driver/MigrationGuide.cpp
    src/Driver/PackageLifecycle.cpp
    src/Driver/PromotionAct.cpp
    src/Driver/ReleaseCandidateFreeze.cpp
    src/Driver/ToolchainContract.cpp
    src/Interop/CHeaderGenerator.cpp
    src/Interop/LibraryMetadata.cpp
    src/Interop/LibraryMetadataMerger.cpp)

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
if(MSVC)
    # PE/COFF otherwise records the wall-clock link timestamp. /Brepro replaces that incidental
    # value with deterministic content-derived data; ordinary builds still differ because their
    # explicit NativeSharp build identity is intentionally unique.
    target_link_options(nsc PRIVATE /Brepro)
endif()

# E9 / Stage 66. El manifiesto instalado lo escribe EL nsc que se acaba de enlazar. Asi CMake no
# mantiene una segunda copia de versiones ABI y un paquete no puede afirmar una identidad distinta
# de la que el compilador realmente verifica. El build id deliberadamente no forma parte del
# contrato: identifica una construccion, pero no cambia compatibilidad.
set(NSC_TOOLCHAIN_CONTRACT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharpToolchain.contract")
set(NSC_COMPATIBILITY_POLICY
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharpCompatibility.policy")
set(NSC_MIGRATION_BASELINE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2Migration.baseline")
set(NSC_BREAKING_CHANGE_LEDGER
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2BreakingChanges.ledger")
set(NSC_MIGRATION_GUIDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2Migration.guide")
set(NSC_RELEASE_CANDIDATE_FREEZE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2RC.freeze")
set(NSC_FINAL_COMPATIBILITY_MATRIX
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2Compatibility.matrix")
set(NSC_FINAL_BASELINE_CONTRACT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2FinalBaseline.contract")
set(NSC_PROMOTION_ACT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp2Promotion.act")
set(NSC_RUNTIME_ABI_CONTRACT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharpRuntime.abi")
set(NSC_WINDOWS_RUNTIME_LIBRARY
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharp.Runtime.Win32.lib")
add_custom_command(TARGET nsc POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND $<TARGET_FILE:nsc> --write-toolchain-contract "${NSC_TOOLCHAIN_CONTRACT}"
    COMMAND $<TARGET_FILE:nsc> --write-compatibility-policy "${NSC_COMPATIBILITY_POLICY}"
    COMMAND $<TARGET_FILE:nsc> --write-migration-baseline "${NSC_MIGRATION_BASELINE}"
    COMMAND $<TARGET_FILE:nsc> --write-breaking-change-ledger "${NSC_BREAKING_CHANGE_LEDGER}"
    COMMAND $<TARGET_FILE:nsc> --write-migration-guide "${NSC_MIGRATION_GUIDE}"
    COMMAND $<TARGET_FILE:nsc> --write-release-candidate-freeze "${NSC_RELEASE_CANDIDATE_FREEZE}"
    COMMAND $<TARGET_FILE:nsc> --write-compatibility-matrix "${NSC_FINAL_COMPATIBILITY_MATRIX}"
    COMMAND $<TARGET_FILE:nsc> --write-final-baseline-contract "${NSC_FINAL_BASELINE_CONTRACT}"
    COMMAND $<TARGET_FILE:nsc> --write-promotion-act "${NSC_PROMOTION_ACT}"
    COMMAND $<TARGET_FILE:nsc> --write-runtime-abi-contract "${NSC_RUNTIME_ABI_CONTRACT}"
    COMMAND $<TARGET_FILE:nsc> --write-windows-runtime-library "${NSC_WINDOWS_RUNTIME_LIBRARY}"
    BYPRODUCTS "${NSC_TOOLCHAIN_CONTRACT}"
               "${NSC_COMPATIBILITY_POLICY}"
               "${NSC_MIGRATION_BASELINE}"
               "${NSC_BREAKING_CHANGE_LEDGER}"
               "${NSC_MIGRATION_GUIDE}"
               "${NSC_RELEASE_CANDIDATE_FREEZE}"
               "${NSC_FINAL_COMPATIBILITY_MATRIX}"
               "${NSC_FINAL_BASELINE_CONTRACT}"
               "${NSC_PROMOTION_ACT}"
               "${NSC_RUNTIME_ABI_CONTRACT}"
               "${NSC_WINDOWS_RUNTIME_LIBRARY}"
    COMMENT "Generando contrato reproducible del toolchain NativeSharp"
    VERBATIM)

# E9 / Stage 64. Esta lista describe el payload esperado por la version que se esta construyendo.
# No se obtiene enumerando el prefijo despues de instalar: hacerlo asi legitimaria como actuales
# los residuos de una version anterior. Las rutas se ordenan y normalizan al configurar; CMake
# vuelve a configurarse si cambia el arbol publico de lib/, spec/ o docs/1.0.1.
file(GLOB_RECURSE NSC_INSTALLATION_LIBRARY_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*")
file(GLOB_RECURSE NSC_INSTALLATION_SPECIFICATION_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/spec"
    "${CMAKE_CURRENT_SOURCE_DIR}/spec/*")
file(GLOB_RECURSE NSC_INSTALLATION_DOCUMENTATION_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/docs"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/*")
set(NSC_INSTALLATION_EXPECTED_PATHS
    "bin/nsc.exe"
    "lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib"
    "share/nativesharp/runtime/NativeSharpRuntime.abi"
    "share/nativesharp/toolchain/NativeSharpInstallation.files"
    "share/nativesharp/toolchain/NativeSharpCompatibility.policy"
    "share/nativesharp/toolchain/NativeSharp2Migration.baseline"
    "share/nativesharp/toolchain/NativeSharp2BreakingChanges.ledger"
    "share/nativesharp/toolchain/NativeSharp2Migration.guide"
    "share/nativesharp/toolchain/NativeSharp2RC.freeze"
    "share/nativesharp/toolchain/NativeSharp2Compatibility.matrix"
    "share/nativesharp/toolchain/NativeSharp2FinalBaseline.contract"
    "share/nativesharp/toolchain/NativeSharp2Promotion.act"
    "share/nativesharp/toolchain/NativeSharpToolchain.contract")
foreach(_nsc_install_file IN LISTS NSC_INSTALLATION_LIBRARY_FILES)
    list(APPEND NSC_INSTALLATION_EXPECTED_PATHS "lib/${_nsc_install_file}")
endforeach()
foreach(_nsc_install_file IN LISTS NSC_INSTALLATION_SPECIFICATION_FILES)
    list(APPEND NSC_INSTALLATION_EXPECTED_PATHS "share/nativesharp/spec/${_nsc_install_file}")
endforeach()
foreach(_nsc_install_file IN LISTS NSC_INSTALLATION_DOCUMENTATION_FILES)
    list(APPEND NSC_INSTALLATION_EXPECTED_PATHS "share/nativesharp/docs/${_nsc_install_file}")
endforeach()
list(SORT NSC_INSTALLATION_EXPECTED_PATHS)
set(NSC_INSTALLATION_FILE_LIST
    "${CMAKE_CURRENT_BINARY_DIR}/generated/NativeSharpInstallation.files")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${NSC_INSTALLATION_FILE_LIST}" "NSFILES 1\n")
foreach(_nsc_install_path IN LISTS NSC_INSTALLATION_EXPECTED_PATHS)
    file(APPEND "${NSC_INSTALLATION_FILE_LIST}" "${_nsc_install_path}\n")
endforeach()

# "--deploy": instala el compilador ya construido en la ruta de instalacion elegida (p. ej.
# C:\native-sharp), junto con el arbol lib/ ENTERO -- las bibliotecas .ns
# (NativeSharp.Core/NativeSharp.Core.Threading/NativeSharp.Core.Collections.Generic/
# NativeSharp.Core.IO/NativeSharp.Core.Diagnostics) que la distribucion oficial todavia consume
# via ProjectReference. Desde 1.0, nsc tambien produce y consume bibliotecas precompiladas mediante
# LibraryReference (.lib + .nsmeta y, para SharedLibrary, .dll); este install conserva Core como
# fuente hasta que su empaquetado oficial migre a esos productos -- ver
# claude/05_metodo_de_trabajo_y_archivo_historico.md, "Convencion: el zip incremental siempre
# incluye lib/ completo" para el porque. Por eso esas fuentes necesitan viajar junto al ejecutable, no solo
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
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spec/ DESTINATION share/nativesharp/spec)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/
    DESTINATION share/nativesharp/docs)
install(FILES "${NSC_TOOLCHAIN_CONTRACT}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_COMPATIBILITY_POLICY}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_MIGRATION_BASELINE}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_BREAKING_CHANGE_LEDGER}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_MIGRATION_GUIDE}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_RELEASE_CANDIDATE_FREEZE}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_FINAL_COMPATIBILITY_MATRIX}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_FINAL_BASELINE_CONTRACT}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_PROMOTION_ACT}" DESTINATION share/nativesharp/toolchain)
install(FILES "${NSC_RUNTIME_ABI_CONTRACT}" DESTINATION share/nativesharp/runtime)
install(FILES "${NSC_WINDOWS_RUNTIME_LIBRARY}" DESTINATION lib/nativesharp/runtime)
install(FILES "${NSC_INSTALLATION_FILE_LIST}" DESTINATION share/nativesharp/toolchain)
if(WIN32)
    # La verificacion corre al FINAL: no basta que CMake haya intentado copiar los archivos. El
    # nsc ya instalado valida el contrato y la superficie minima desde el prefijo efectivo, incluido
    # uno reemplazado mediante `cmake --install --prefix`.
    install(SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyInstalledNativeSharp.cmake")
endif()

# =====================================================================================
# NSC_TEST_COMPILER -- EL COMPILADOR QUE MIDE CTEST.
#
# Los 117 tests de mas abajo invocaban el OBJETIVO `nsc`, y CMake resuelve un objetivo al
# binario del ARBOL DE BUILD: build-clang/bin/nsc.exe. Eso ata la bateria a un artefacto
# intermedio en vez de al que se entrega. `cmake --install` copia ese binario a
# C:/native-sharp/bin, y entre los dos caben una instalacion que fallo, un install que se
# salto, o un arbol de build rehecho despues. Lo que se mide tiene que ser lo instalado,
# porque es lo que un proyecto de fuera va a usar.
#
# EL 2026-09-04 ESO SALIO A LA SUPERFICIE. test_nsc.bat construye el objetivo `pruebas`,
# que EXCLUYE `nsc` a proposito -- para no cambiar el compilador que se esta midiendo. Sin
# `build-clang/bin/nsc.exe`, esos tests salieron `***Not Run` con "Could not find executable
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
# Y dentro de la propia bateria queda `nsc.version`, el primero de los 117: comprueba
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
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_test(
        NAME nsc.selfhost.bootstrap-infrastructure
        COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/selfhost/test_bootstrap_infrastructure.py")
    set_tests_properties(
        nsc.selfhost.bootstrap-infrastructure
        PROPERTIES ENVIRONMENT
            "PYTHONDONTWRITEBYTECODE=1;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
        NAME nsc.selfhost.bootstrap-profile-contract
        COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/selfhost/test_bootstrap_profile.py")
    set_tests_properties(
        nsc.selfhost.bootstrap-profile-contract
        PROPERTIES ENVIRONMENT
            "PYTHONDONTWRITEBYTECODE=1;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
        NAME nsc.selfhost.bootstrap-profile-positive
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-profile"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpBootstrapProfile.cmake")
    set_tests_properties(nsc.selfhost.bootstrap-profile-positive PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.compiler-host-workload
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-host-workload"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpCompilerHostWorkload.cmake")
    set_tests_properties(nsc.selfhost.compiler-host-workload PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.llvm-c-bridge
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-llvm-c-bridge"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpLlvmCBridge.cmake")
    set_tests_properties(nsc.selfhost.llvm-c-bridge PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.llvm-backend
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-llvm-backend"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpLlvmBackend.cmake")
    set_tests_properties(nsc.selfhost.llvm-backend PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.project-driver
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-project-driver"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpProjectDriver.cmake")
    set_tests_properties(nsc.selfhost.project-driver PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.first-generation
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-first-generation"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpFirstGeneration.cmake")
    set_tests_properties(nsc.selfhost.first-generation PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.image-replay-retired
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-image-replay-retired"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpImageReplayRetired.cmake")
    set_tests_properties(nsc.selfhost.image-replay-retired PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-derived-backend
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-derived-backend"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceDerivedBackend.cmake")
    set_tests_properties(nsc.selfhost.source-derived-backend PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-functions
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-functions"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceFunctions.cmake")
    set_tests_properties(nsc.selfhost.source-functions PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-control-flow
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-control-flow"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceControlFlow.cmake")
    set_tests_properties(nsc.selfhost.source-control-flow PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-loop-control
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-loop-control"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceLoopControl.cmake")
    set_tests_properties(nsc.selfhost.source-loop-control PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-bool-void
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-bool-void"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceBoolVoid.cmake")
    set_tests_properties(nsc.selfhost.source-bool-void PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-expressions
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-expressions"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceExpressions.cmake")
    set_tests_properties(nsc.selfhost.source-expressions PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-integer-widths
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-integer-widths"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceIntegerWidths.cmake")
    set_tests_properties(nsc.selfhost.source-integer-widths PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-pointers
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-pointers"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourcePointers.cmake")
    set_tests_properties(nsc.selfhost.source-pointers PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-floating-scalars
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-floating-scalars"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceFloatingScalars.cmake")
    set_tests_properties(nsc.selfhost.source-floating-scalars PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.source-graph
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-source-graph"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSourceGraph.cmake")
    set_tests_properties(nsc.selfhost.source-graph PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.module-set
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-module-set"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpModuleSet.cmake")
    set_tests_properties(nsc.selfhost.module-set PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.backend-general
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-backend-general"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpBackendGeneral.cmake")
    set_tests_properties(nsc.selfhost.backend-general PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.program-ir
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-program-ir"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpProgramIr.cmake")
    set_tests_properties(nsc.selfhost.program-ir PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.backend-general)

    add_test(
        NAME nsc.selfhost.data-layout
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-data-layout"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpDataLayout.cmake")
    set_tests_properties(nsc.selfhost.data-layout PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.program-ir)

    add_test(
        NAME nsc.selfhost.object-model
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-object-model"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpObjectModel.cmake")
    set_tests_properties(nsc.selfhost.object-model PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.data-layout)

    add_test(
        NAME nsc.selfhost.native-text
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-native-text"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpNativeText.cmake")
    set_tests_properties(nsc.selfhost.native-text PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.object-model)

    add_test(
        NAME nsc.selfhost.arrays-memory
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-arrays-memory"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpArraysMemory.cmake")
    set_tests_properties(nsc.selfhost.arrays-memory PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.native-text)

    add_test(
        NAME nsc.selfhost.interop-base
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-interop-base"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpInteropBase.cmake")
    set_tests_properties(nsc.selfhost.interop-base PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.arrays-memory)

    add_test(
        NAME nsc.selfhost.generics-aot
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-generics-aot"
            "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
            "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpGenericsAot.cmake")
    set_tests_properties(nsc.selfhost.generics-aot PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.interop-base)

    add_test(NAME nsc.selfhost.ownership-cleanup COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-ownership-cleanup"
        "-DLINKER_EXECUTABLE=${CMAKE_LINKER}" "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpOwnershipCleanup.cmake")
    set_tests_properties(nsc.selfhost.ownership-cleanup PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.generics-aot)

    add_test(NAME nsc.selfhost.callable-graph COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-callable-graph"
        "-DLINKER_EXECUTABLE=${CMAKE_LINKER}" "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpCallableGraph.cmake")
    set_tests_properties(nsc.selfhost.callable-graph PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.ownership-cleanup)

    add_test(NAME nsc.selfhost.bootstrap-surface COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-bootstrap-surface"
        "-DLINKER_EXECUTABLE=${CMAKE_LINKER}" "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpBootstrapSurface.cmake")
    set_tests_properties(nsc.selfhost.bootstrap-surface PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.callable-graph)

    add_test(NAME nsc.selfhost.eh-regions COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-eh-regions"
        "-DLINKER_EXECUTABLE=${CMAKE_LINKER}" "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpEhRegions.cmake")
    set_tests_properties(nsc.selfhost.eh-regions PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.bootstrap-surface)

    add_test(NAME nsc.selfhost.driver-products COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-driver-products"
        "-DLINKER_EXECUTABLE=${CMAKE_LINKER}" "-DNSLINK_EXECUTABLE=${CMAKE_INSTALL_PREFIX}/bin/nslink.exe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpDriverProducts.cmake")
    set_tests_properties(nsc.selfhost.driver-products PROPERTIES RUN_SERIAL TRUE DEPENDS nsc.selfhost.eh-regions)

    add_test(
        NAME nsc.selfhost.promotion-readiness
        COMMAND "${CMAKE_COMMAND}" "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}" "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-promotion-readiness"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpPromotionReadiness.cmake")
    set_tests_properties(nsc.selfhost.promotion-readiness PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.tanda16-closure
        COMMAND "${CMAKE_COMMAND}" "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-tanda16-closure"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSelfHostClosure.cmake")
    set_tests_properties(nsc.selfhost.tanda16-closure PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.nsc-next-skeleton
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-nsc-next-skeleton"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpNextSkeleton.cmake")
    set_tests_properties(nsc.selfhost.nsc-next-skeleton PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.frontend
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-frontend"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpFrontend.cmake")
    set_tests_properties(nsc.selfhost.frontend PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.semantics
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-semantics"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpSemantics.cmake")
    set_tests_properties(nsc.selfhost.semantics PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.selfhost.ir
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-ir"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpIr.cmake")
    set_tests_properties(nsc.selfhost.ir PROPERTIES RUN_SERIAL TRUE)

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
    nsc_add_unit_test(
        nsc_integrated_assembly_grammar_tests
        tests/unit/IntegratedAssemblyGrammarTests.cpp
        nsc.integrated-assembly.grammar)
    add_executable(
        nsc_integrated_assembly_mir_tests
        tests/unit/IntegratedAssemblyMirTests.cpp)
    target_link_libraries(
        nsc_integrated_assembly_mir_tests PRIVATE nsc_llvm nsc_frontend nsc_compilation)
    target_compile_features(nsc_integrated_assembly_mir_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_integrated_assembly_mir_tests)
    add_test(NAME nsc.integrated-assembly.mir COMMAND nsc_integrated_assembly_mir_tests)
    nsc_add_unit_test(nsc_namespace_syntax_tests tests/unit/NamespaceSyntaxTests.cpp nsc.namespace-syntax)
    nsc_add_unit_test(nsc_class_declaration_tests tests/unit/ClassDeclarationTests.cpp nsc.class-declarations)
    nsc_add_unit_test(nsc_class_member_syntax_tests tests/unit/ClassMemberSyntaxTests.cpp nsc.class-member-syntax)
    nsc_add_unit_test(nsc_constructor_syntax_tests tests/unit/ConstructorSyntaxTests.cpp nsc.constructor-syntax)
    nsc_add_unit_test(
        nsc_item317_placement_construction_syntax_tests
        tests/unit/Item317PlacementConstructionSyntaxTests.cpp
        nsc.item317.placement-construction-syntax)
    add_executable(
        nsc_item318_placement_construction_lowering_tests
        tests/unit/Item318PlacementConstructionLoweringTests.cpp)
    target_link_libraries(nsc_item318_placement_construction_lowering_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item318_placement_construction_lowering_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item318_placement_construction_lowering_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item318_placement_construction_lowering_tests)
    add_test(
        NAME nsc.item318.placement-construction-lowering
        COMMAND nsc_item318_placement_construction_lowering_tests)
    add_executable(
        nsc_item319_explicit_object_destruction_tests
        tests/unit/Item319ExplicitObjectDestructionTests.cpp)
    target_link_libraries(nsc_item319_explicit_object_destruction_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item319_explicit_object_destruction_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item319_explicit_object_destruction_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item319_explicit_object_destruction_tests)
    add_test(
        NAME nsc.item319.explicit-object-destruction
        COMMAND nsc_item319_explicit_object_destruction_tests)
    add_executable(
        nsc_item320_object_storage_ownership_tests
        tests/unit/Item320ObjectStorageOwnershipTests.cpp)
    target_link_libraries(nsc_item320_object_storage_ownership_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item320_object_storage_ownership_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item320_object_storage_ownership_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item320_object_storage_ownership_tests)
    add_test(
        NAME nsc.item320.object-storage-ownership
        COMMAND nsc_item320_object_storage_ownership_tests)
    add_executable(
        nsc_item321_noheap_context_tests
        tests/unit/Item321NoHeapContextTests.cpp)
    target_link_libraries(nsc_item321_noheap_context_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item321_noheap_context_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item321_noheap_context_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item321_noheap_context_tests)
    add_test(
        NAME nsc.item321.noheap-context
        COMMAND nsc_item321_noheap_context_tests)
    add_executable(
        nsc_item322_noheap_propagation_tests
        tests/unit/Item322NoHeapPropagationTests.cpp)
    target_link_libraries(nsc_item322_noheap_propagation_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item322_noheap_propagation_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item322_noheap_propagation_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item322_noheap_propagation_tests)
    add_test(
        NAME nsc.item322.noheap-propagation
        COMMAND nsc_item322_noheap_propagation_tests)
    add_executable(
        nsc_item323_stack_class_escape_analysis_tests
        tests/unit/Item323StackClassEscapeAnalysisTests.cpp)
    target_link_libraries(nsc_item323_stack_class_escape_analysis_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item323_stack_class_escape_analysis_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item323_stack_class_escape_analysis_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item323_stack_class_escape_analysis_tests)
    add_test(
        NAME nsc.item323.stack-class-escape-analysis
        COMMAND nsc_item323_stack_class_escape_analysis_tests)
    add_executable(
        nsc_item324_anonymous_struct_tests
        tests/unit/Item324AnonymousStructTests.cpp)
    target_link_libraries(nsc_item324_anonymous_struct_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item324_anonymous_struct_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item324_anonymous_struct_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item324_anonymous_struct_tests)
    add_test(
        NAME nsc.item324.anonymous-struct
        COMMAND nsc_item324_anonymous_struct_tests)
    add_executable(
        nsc_item325_anonymous_union_tests
        tests/unit/Item325AnonymousUnionTests.cpp)
    target_link_libraries(nsc_item325_anonymous_union_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item325_anonymous_union_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item325_anonymous_union_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item325_anonymous_union_tests)
    add_test(
        NAME nsc.item325.anonymous-union
        COMMAND nsc_item325_anonymous_union_tests)
    add_executable(
        nsc_item326_anonymous_member_promotion_tests
        tests/unit/Item326AnonymousMemberPromotionTests.cpp)
    target_link_libraries(nsc_item326_anonymous_member_promotion_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item326_anonymous_member_promotion_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item326_anonymous_member_promotion_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item326_anonymous_member_promotion_tests)
    add_test(
        NAME nsc.item326.anonymous-member-promotion
        COMMAND nsc_item326_anonymous_member_promotion_tests)
    add_executable(
        nsc_item327_bitfield_syntax_tests
        tests/unit/Item327BitFieldSyntaxTests.cpp)
    target_link_libraries(nsc_item327_bitfield_syntax_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item327_bitfield_syntax_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item327_bitfield_syntax_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item327_bitfield_syntax_tests)
    add_test(
        NAME nsc.item327.bitfield-syntax
        COMMAND nsc_item327_bitfield_syntax_tests)
    add_executable(
        nsc_item328_msvc_bitfield_layout_tests
        tests/unit/Item328MsvcBitFieldLayoutTests.cpp)
    target_link_libraries(nsc_item328_msvc_bitfield_layout_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item328_msvc_bitfield_layout_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item328_msvc_bitfield_layout_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item328_msvc_bitfield_layout_tests)
    add_test(
        NAME nsc.item328.msvc-bitfield-layout
        COMMAND nsc_item328_msvc_bitfield_layout_tests)
    add_executable(
        nsc_item329_bitfield_access_tests
        tests/unit/Item329BitFieldAccessTests.cpp)
    target_link_libraries(nsc_item329_bitfield_access_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item329_bitfield_access_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item329_bitfield_access_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item329_bitfield_access_tests)
    add_test(
        NAME nsc.item329.bitfield-access
        COMMAND nsc_item329_bitfield_access_tests)
    add_executable(
        nsc_item330_volatile_bitfield_tests
        tests/unit/Item330VolatileBitFieldTests.cpp)
    target_link_libraries(nsc_item330_volatile_bitfield_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item330_volatile_bitfield_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item330_volatile_bitfield_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item330_volatile_bitfield_tests)
    add_test(
        NAME nsc.item330.volatile-bitfields
        COMMAND nsc_item330_volatile_bitfield_tests)
    add_executable(
        nsc_item331_variadic_c_declaration_tests
        tests/unit/Item331VariadicCDeclarationTests.cpp)
    target_link_libraries(nsc_item331_variadic_c_declaration_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item331_variadic_c_declaration_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item331_variadic_c_declaration_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item331_variadic_c_declaration_tests)
    add_test(
        NAME nsc.item331.variadic-c-declarations
        COMMAND nsc_item331_variadic_c_declaration_tests)
    add_executable(
        nsc_item332_c_vararg_promotion_tests
        tests/unit/Item332CVarargPromotionTests.cpp)
    target_link_libraries(nsc_item332_c_vararg_promotion_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item332_c_vararg_promotion_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item332_c_vararg_promotion_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item332_c_vararg_promotion_tests)
    add_test(
        NAME nsc.item332.c-vararg-promotions
        COMMAND nsc_item332_c_vararg_promotion_tests)
    add_executable(
        nsc_item334_declarative_sdk_format_tests
        tests/unit/Item334DeclarativeSdkFormatTests.cpp)
    target_link_libraries(nsc_item334_declarative_sdk_format_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_item334_declarative_sdk_format_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item334_declarative_sdk_format_tests)
    add_test(
        NAME nsc.item334.declarative-sdk-format
        COMMAND nsc_item334_declarative_sdk_format_tests)
    add_executable(
        nsc_item335_sdk_mapping_tests
        tests/unit/Item335SdkMappingTests.cpp)
    target_link_libraries(nsc_item335_sdk_mapping_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_item335_sdk_mapping_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item335_sdk_mapping_tests)
    add_test(
        NAME nsc.item335.sdk-mapping
        COMMAND nsc_item335_sdk_mapping_tests)
    add_executable(
        nsc_item336_sdk_target_selection_tests
        tests/unit/Item336SdkTargetSelectionTests.cpp)
    target_link_libraries(nsc_item336_sdk_target_selection_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_item336_sdk_target_selection_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item336_sdk_target_selection_tests)
    add_test(
        NAME nsc.item336.sdk-target-selection
        COMMAND nsc_item336_sdk_target_selection_tests)
    add_executable(
        nsc_item337_sdk_diagnostics_tests
        tests/unit/Item337SdkDiagnosticsTests.cpp)
    target_link_libraries(nsc_item337_sdk_diagnostics_tests PRIVATE nsc_compilation)
    target_compile_features(nsc_item337_sdk_diagnostics_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item337_sdk_diagnostics_tests)
    add_test(
        NAME nsc.item337.sdk-diagnostics
        COMMAND nsc_item337_sdk_diagnostics_tests)
    add_executable(
        nsc_item338_raw_com_vtable_tests
        tests/unit/Item338RawComVtableTests.cpp)
    target_link_libraries(nsc_item338_raw_com_vtable_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item338_raw_com_vtable_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item338_raw_com_vtable_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item338_raw_com_vtable_tests)
    add_test(
        NAME nsc.item338.raw-com-vtable
        COMMAND nsc_item338_raw_com_vtable_tests)
    add_executable(
        nsc_item339_com_vtable_production_tests
        tests/unit/Item339ComVtableProductionTests.cpp)
    target_link_libraries(nsc_item339_com_vtable_production_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item339_com_vtable_production_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item339_com_vtable_production_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item339_com_vtable_production_tests)
    add_test(
        NAME nsc.item339.com-vtable-production
        COMMAND nsc_item339_com_vtable_production_tests)
    add_executable(
        nsc_item340_com_function_pointer_tests
        tests/unit/Item340ComFunctionPointerTests.cpp)
    target_link_libraries(nsc_item340_com_function_pointer_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item340_com_function_pointer_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item340_com_function_pointer_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item340_com_function_pointer_tests)
    add_test(
        NAME nsc.item340.com-function-pointers
        COMMAND nsc_item340_com_function_pointer_tests)
    add_executable(
        nsc_item341_exception_safe_com_thunk_tests
        tests/unit/Item341ExceptionSafeComThunkTests.cpp)
    target_link_libraries(nsc_item341_exception_safe_com_thunk_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item341_exception_safe_com_thunk_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item341_exception_safe_com_thunk_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item341_exception_safe_com_thunk_tests)
    add_test(
        NAME nsc.item341.exception-safe-com-thunk
        COMMAND nsc_item341_exception_safe_com_thunk_tests)
    add_executable(
        nsc_item342_aggregate_abi_matrix_tests
        tests/unit/Item342AggregateAbiMatrixTests.cpp)
    target_link_libraries(nsc_item342_aggregate_abi_matrix_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item342_aggregate_abi_matrix_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item342_aggregate_abi_matrix_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item342_aggregate_abi_matrix_tests)
    add_test(
        NAME nsc.item342.aggregate-abi-matrix
        COMMAND nsc_item342_aggregate_abi_matrix_tests)
    add_executable(
        nsc_item343_callback_output_parameter_tests
        tests/unit/Item343CallbackOutputParameterTests.cpp)
    target_link_libraries(nsc_item343_callback_output_parameter_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item343_callback_output_parameter_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item343_callback_output_parameter_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item343_callback_output_parameter_tests)
    add_test(
        NAME nsc.item343.callback-output-parameters
        COMMAND nsc_item343_callback_output_parameter_tests)
    add_executable(
        nsc_item344_vectorcall_tests
        tests/unit/Item344VectorCallTests.cpp)
    target_link_libraries(nsc_item344_vectorcall_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item344_vectorcall_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item344_vectorcall_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item344_vectorcall_tests)
    add_test(NAME nsc.item344.vectorcall COMMAND nsc_item344_vectorcall_tests)
    add_executable(
        nsc_item345_vectorcall_hva_tests
        tests/unit/Item345VectorCallHvaTests.cpp)
    target_link_libraries(nsc_item345_vectorcall_hva_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item345_vectorcall_hva_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item345_vectorcall_hva_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item345_vectorcall_hva_tests)
    add_test(NAME nsc.item345.vectorcall-hva COMMAND nsc_item345_vectorcall_hva_tests)
    add_executable(
        nsc_item346_consumer_target_tests
        tests/unit/Item346ConsumerTargetTests.cpp)
    target_link_libraries(nsc_item346_consumer_target_tests PRIVATE
        nsc_frontend nsc_llvm nsc_target)
    target_compile_features(nsc_item346_consumer_target_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item346_consumer_target_tests)
    add_test(NAME nsc.item346.consumer-target COMMAND nsc_item346_consumer_target_tests)
    add_executable(
        nsc_item347_integer_vector_128_tests
        tests/unit/Item347IntegerVector128Tests.cpp)
    target_link_libraries(nsc_item347_integer_vector_128_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item347_integer_vector_128_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item347_integer_vector_128_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item347_integer_vector_128_tests)
    add_test(
        NAME nsc.item347.integer-vector-128
        COMMAND nsc_item347_integer_vector_128_tests)
    add_executable(
        nsc_item348_floating_vector_256_tests
        tests/unit/Item348FloatingVector256Tests.cpp)
    target_link_libraries(nsc_item348_floating_vector_256_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item348_floating_vector_256_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item348_floating_vector_256_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item348_floating_vector_256_tests)
    add_test(
        NAME nsc.item348.floating-vector-256
        COMMAND nsc_item348_floating_vector_256_tests)
    add_executable(
        nsc_item349_integer_vector_256_tests
        tests/unit/Item349IntegerVector256Tests.cpp)
    target_link_libraries(nsc_item349_integer_vector_256_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item349_integer_vector_256_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item349_integer_vector_256_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item349_integer_vector_256_tests)
    add_test(
        NAME nsc.item349.integer-vector-256
        COMMAND nsc_item349_integer_vector_256_tests)
    add_executable(
        nsc_item350_aligned_simd_memory_tests
        tests/unit/Item350AlignedSimdMemoryTests.cpp)
    target_link_libraries(nsc_item350_aligned_simd_memory_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item350_aligned_simd_memory_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item350_aligned_simd_memory_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item350_aligned_simd_memory_tests)
    add_test(
        NAME nsc.item350.aligned-simd-memory
        COMMAND nsc_item350_aligned_simd_memory_tests)
    add_executable(
        nsc_item351_unaligned_simd_memory_tests
        tests/unit/Item351UnalignedSimdMemoryTests.cpp)
    target_link_libraries(nsc_item351_unaligned_simd_memory_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item351_unaligned_simd_memory_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item351_unaligned_simd_memory_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item351_unaligned_simd_memory_tests)
    add_test(
        NAME nsc.item351.unaligned-simd-memory
        COMMAND nsc_item351_unaligned_simd_memory_tests)
    add_executable(
        nsc_item352_compact_float_vector_tests
        tests/unit/Item352CompactFloatVectorTests.cpp)
    target_link_libraries(nsc_item352_compact_float_vector_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item352_compact_float_vector_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item352_compact_float_vector_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item352_compact_float_vector_tests)
    add_test(NAME nsc.item352.compact-float-vector COMMAND nsc_item352_compact_float_vector_tests)
    add_executable(
        nsc_item353_native_matrix_tests
        tests/unit/Item353NativeMatrixTests.cpp)
    target_link_libraries(nsc_item353_native_matrix_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item353_native_matrix_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item353_native_matrix_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item353_native_matrix_tests)
    add_test(NAME nsc.item353.native-matrix COMMAND nsc_item353_native_matrix_tests)
    add_executable(nsc_item354_half_precision_tests tests/unit/Item354HalfPrecisionTests.cpp)
    target_link_libraries(nsc_item354_half_precision_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item354_half_precision_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item354_half_precision_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item354_half_precision_tests)
    add_test(NAME nsc.item354.half-precision COMMAND nsc_item354_half_precision_tests)
    add_executable(nsc_item355_packed_gpu_format_tests tests/unit/Item355PackedGpuFormatTests.cpp)
    target_link_libraries(nsc_item355_packed_gpu_format_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item355_packed_gpu_format_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item355_packed_gpu_format_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item355_packed_gpu_format_tests)
    add_test(NAME nsc.item355.packed-gpu-format COMMAND nsc_item355_packed_gpu_format_tests)
    add_executable(nsc_item356_simd_shuffle_tests tests/unit/Item356SimdShuffleTests.cpp)
    target_link_libraries(nsc_item356_simd_shuffle_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item356_simd_shuffle_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item356_simd_shuffle_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item356_simd_shuffle_tests)
    add_test(NAME nsc.item356.simd-shuffle COMMAND nsc_item356_simd_shuffle_tests)
    add_executable(nsc_item357_vector_math_tests tests/unit/Item357VectorMathTests.cpp)
    target_link_libraries(nsc_item357_vector_math_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item357_vector_math_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item357_vector_math_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item357_vector_math_tests)
    add_test(NAME nsc.item357.vector-math COMMAND nsc_item357_vector_math_tests)
    add_executable(nsc_item358_vector_root_fma_tests tests/unit/Item358VectorRootFmaTests.cpp)
    target_link_libraries(nsc_item358_vector_root_fma_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item358_vector_root_fma_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item358_vector_root_fma_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item358_vector_root_fma_tests)
    add_test(NAME nsc.item358.vector-root-fma COMMAND nsc_item358_vector_root_fma_tests)
    add_executable(nsc_item359_bit_count_intrinsic_tests tests/unit/Item359BitCountIntrinsicTests.cpp)
    target_link_libraries(nsc_item359_bit_count_intrinsic_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item359_bit_count_intrinsic_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item359_bit_count_intrinsic_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item359_bit_count_intrinsic_tests)
    add_test(NAME nsc.item359.bit-count-intrinsic COMMAND nsc_item359_bit_count_intrinsic_tests)
    add_executable(nsc_item360_rotate_byte_swap_tests tests/unit/Item360RotateByteSwapTests.cpp)
    target_link_libraries(nsc_item360_rotate_byte_swap_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item360_rotate_byte_swap_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item360_rotate_byte_swap_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item360_rotate_byte_swap_tests)
    add_test(NAME nsc.item360.rotate-byte-swap COMMAND nsc_item360_rotate_byte_swap_tests)
    add_executable(nsc_item361_prefetch_pause_tests tests/unit/Item361PrefetchPauseTests.cpp)
    target_link_libraries(nsc_item361_prefetch_pause_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item361_prefetch_pause_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item361_prefetch_pause_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item361_prefetch_pause_tests)
    add_test(NAME nsc.item361.prefetch-pause COMMAND nsc_item361_prefetch_pause_tests)
    add_executable(nsc_item362_cpu_timestamp_tests tests/unit/Item362CpuTimestampTests.cpp)
    target_link_libraries(nsc_item362_cpu_timestamp_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item362_cpu_timestamp_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item362_cpu_timestamp_tests PRIVATE NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item362_cpu_timestamp_tests)
    add_test(NAME nsc.item362.cpu-timestamp COMMAND nsc_item362_cpu_timestamp_tests)

    add_executable(nsc_item363_cpu_feature_selection_tests tests/unit/Item363CpuFeatureSelectionTests.cpp)
    target_link_libraries(nsc_item363_cpu_feature_selection_tests PRIVATE nsc_llvm nsc_target)
    target_compile_features(nsc_item363_cpu_feature_selection_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item363_cpu_feature_selection_tests)
    add_test(NAME nsc.item363.cpu-feature-selection COMMAND nsc_item363_cpu_feature_selection_tests)

    add_executable(nsc_item364_floating_point_mode_tests tests/unit/Item364FloatingPointModeTests.cpp)
    target_link_libraries(nsc_item364_floating_point_mode_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item364_floating_point_mode_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item364_floating_point_mode_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item364_floating_point_mode_tests)
    add_test(NAME nsc.item364.floating-point-modes COMMAND nsc_item364_floating_point_mode_tests)

    add_executable(nsc_item365_fma_contract_policy_tests tests/unit/Item365FmaContractPolicyTests.cpp)
    target_link_libraries(nsc_item365_fma_contract_policy_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item365_fma_contract_policy_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item365_fma_contract_policy_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item365_fma_contract_policy_tests)
    add_test(NAME nsc.item365.fma-contract-policy COMMAND nsc_item365_fma_contract_policy_tests)

    add_executable(nsc_item366_rounding_control_tests tests/unit/Item366RoundingControlTests.cpp)
    target_link_libraries(nsc_item366_rounding_control_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item366_rounding_control_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item366_rounding_control_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item366_rounding_control_tests)
    add_test(NAME nsc.item366.rounding-control COMMAND nsc_item366_rounding_control_tests)

    add_executable(nsc_item367_denormal_control_tests tests/unit/Item367DenormalControlTests.cpp)
    target_link_libraries(nsc_item367_denormal_control_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item367_denormal_control_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item367_denormal_control_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item367_denormal_control_tests)
    add_test(NAME nsc.item367.denormal-control COMMAND nsc_item367_denormal_control_tests)

    add_executable(nsc_item368_thread_fp_state_tests tests/unit/Item368ThreadFloatingPointStateTests.cpp)
    target_link_libraries(nsc_item368_thread_fp_state_tests PRIVATE nsc_llvm)
    target_compile_features(nsc_item368_thread_fp_state_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item368_thread_fp_state_tests)
    add_test(NAME nsc.item368.thread-fp-state COMMAND nsc_item368_thread_fp_state_tests)

    add_executable(nsc_item369_deterministic_math_tests tests/unit/Item369DeterministicMathTests.cpp)
    target_link_libraries(nsc_item369_deterministic_math_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item369_deterministic_math_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item369_deterministic_math_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_item369_deterministic_math_tests)
    add_test(NAME nsc.item369.deterministic-math COMMAND nsc_item369_deterministic_math_tests)

    add_executable(nsc_item370_named_cross_process_sync_tests
        tests/unit/Item370NamedCrossProcessSynchronizationTests.cpp)
    target_link_libraries(nsc_item370_named_cross_process_sync_tests PRIVATE nsc_driver nsc_runtime)
    target_compile_features(nsc_item370_named_cross_process_sync_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item370_named_cross_process_sync_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item370_named_cross_process_sync_tests)
    add_test(NAME nsc.item370.named-cross-process-sync
        COMMAND nsc_item370_named_cross_process_sync_tests)

    add_executable(nsc_item371_native_thread_pool_tests
        tests/unit/Item371NativeThreadPoolTests.cpp)
    target_link_libraries(nsc_item371_native_thread_pool_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item371_native_thread_pool_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item371_native_thread_pool_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item371_native_thread_pool_tests)
    add_test(NAME nsc.item371.native-thread-pool COMMAND nsc_item371_native_thread_pool_tests)

    add_executable(nsc_item372_typed_jobs_tests tests/unit/Item372TypedJobsTests.cpp)
    target_link_libraries(nsc_item372_typed_jobs_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item372_typed_jobs_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item372_typed_jobs_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item372_typed_jobs_tests)
    add_test(NAME nsc.item372.typed-jobs COMMAND nsc_item372_typed_jobs_tests)

    add_executable(nsc_item373_job_dependencies_cancellation_tests
        tests/unit/Item373JobDependenciesCancellationTests.cpp)
    target_link_libraries(nsc_item373_job_dependencies_cancellation_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item373_job_dependencies_cancellation_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item373_job_dependencies_cancellation_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item373_job_dependencies_cancellation_tests)
    add_test(NAME nsc.item373.job-dependencies-cancellation
        COMMAND nsc_item373_job_dependencies_cancellation_tests)

    add_executable(nsc_item374_per_worker_queues_tests
        tests/unit/Item374PerWorkerQueuesTests.cpp)
    target_link_libraries(nsc_item374_per_worker_queues_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item374_per_worker_queues_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item374_per_worker_queues_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item374_per_worker_queues_tests)
    add_test(NAME nsc.item374.per-worker-queues COMMAND nsc_item374_per_worker_queues_tests)

    add_executable(nsc_item375_work_stealing_tests tests/unit/Item375WorkStealingTests.cpp)
    target_link_libraries(nsc_item375_work_stealing_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item375_work_stealing_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item375_work_stealing_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item375_work_stealing_tests)
    add_test(NAME nsc.item375.work-stealing COMMAND nsc_item375_work_stealing_tests)

    add_executable(nsc_item376_worker_fp_initialization_tests
        tests/unit/Item376WorkerFloatingPointInitializationTests.cpp)
    target_link_libraries(nsc_item376_worker_fp_initialization_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item376_worker_fp_initialization_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item376_worker_fp_initialization_tests)
    add_test(NAME nsc.item376.worker-fp-initialization
        COMMAND nsc_item376_worker_fp_initialization_tests)

    add_executable(nsc_item385_comptime_core_tests tests/unit/Item385ComptimeCoreTests.cpp)
    target_link_libraries(nsc_item385_comptime_core_tests PRIVATE nsc_frontend)
    target_compile_features(nsc_item385_comptime_core_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item385_comptime_core_tests)
    add_test(NAME nsc.item385.comptime-core COMMAND nsc_item385_comptime_core_tests)

    add_executable(nsc_item386_comptime_sandbox_tests tests/unit/Item386ComptimeSandboxTests.cpp)
    target_link_libraries(nsc_item386_comptime_sandbox_tests PRIVATE nsc_frontend)
    target_compile_features(nsc_item386_comptime_sandbox_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item386_comptime_sandbox_tests)
    add_test(NAME nsc.item386.comptime-sandbox COMMAND nsc_item386_comptime_sandbox_tests)

    add_executable(nsc_item387_comptime_cache_tests tests/unit/Item387ComptimeCacheTests.cpp)
    target_link_libraries(nsc_item387_comptime_cache_tests PRIVATE nsc_frontend)
    target_compile_features(nsc_item387_comptime_cache_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item387_comptime_cache_tests)
    add_test(NAME nsc.item387.comptime-cache COMMAND nsc_item387_comptime_cache_tests)

    add_executable(nsc_item388_comptime_reproducibility_tests
        tests/unit/Item388ComptimeReproducibilityTests.cpp)
    target_link_libraries(nsc_item388_comptime_reproducibility_tests PRIVATE nsc_frontend)
    target_compile_features(nsc_item388_comptime_reproducibility_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item388_comptime_reproducibility_tests)
    add_test(NAME nsc.item388.comptime-reproducibility
        COMMAND nsc_item388_comptime_reproducibility_tests)

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
    target_link_libraries(nsc_project_oop_lowering_tests PRIVATE nsc_linker nsc_llvm nsc_frontend nsc_target)
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

    add_executable(nsc_static_assert_tests tests/unit/StaticAssertTests.cpp)
    target_link_libraries(nsc_static_assert_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_static_assert_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_static_assert_tests)
    add_test(NAME nsc.language.static-assert COMMAND nsc_static_assert_tests)

    add_executable(nsc_offsetof_tests tests/unit/OffsetofTests.cpp)
    target_link_libraries(nsc_offsetof_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_offsetof_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_offsetof_tests)
    add_test(NAME nsc.language.offsetof COMMAND nsc_offsetof_tests)

    add_executable(nsc_fixed_multidimensional_array_tests tests/unit/FixedMultidimensionalArrayTests.cpp)
    target_link_libraries(nsc_fixed_multidimensional_array_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_fixed_multidimensional_array_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_fixed_multidimensional_array_tests)
    add_test(NAME nsc.language.fixed-multidimensional-arrays COMMAND nsc_fixed_multidimensional_array_tests)

    add_executable(nsc_multidimensional_index_tests tests/unit/MultidimensionalIndexTests.cpp)
    target_link_libraries(nsc_multidimensional_index_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_multidimensional_index_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_multidimensional_index_tests)
    add_test(NAME nsc.language.multidimensional-indexing COMMAND nsc_multidimensional_index_tests)

    add_executable(nsc_variable_length_array_tests tests/unit/VariableLengthArrayTests.cpp)
    target_link_libraries(nsc_variable_length_array_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_variable_length_array_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_variable_length_array_tests)
    add_test(NAME nsc.language.variable-length-arrays COMMAND nsc_variable_length_array_tests)

    add_executable(nsc_explicit_field_alignment_tests tests/unit/ExplicitFieldAlignmentTests.cpp)
    target_link_libraries(nsc_explicit_field_alignment_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_explicit_field_alignment_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_explicit_field_alignment_tests)
    add_test(NAME nsc.language.explicit-field-alignment COMMAND nsc_explicit_field_alignment_tests)

    add_executable(nsc_flexible_array_tests tests/unit/FlexibleArrayTests.cpp)
    target_link_libraries(nsc_flexible_array_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_flexible_array_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_flexible_array_tests)
    add_test(NAME nsc.language.flexible-arrays COMMAND nsc_flexible_array_tests)

    add_executable(nsc_readonly_struct_tests tests/unit/ReadonlyStructTests.cpp)
    target_link_libraries(nsc_readonly_struct_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_readonly_struct_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_readonly_struct_tests)
    add_test(NAME nsc.language.readonly-structs COMMAND nsc_readonly_struct_tests)

    add_executable(nsc_readonly_receiver_tests tests/unit/ReadonlyReceiverTests.cpp)
    target_link_libraries(nsc_readonly_receiver_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_readonly_receiver_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_readonly_receiver_tests)
    add_test(NAME nsc.language.readonly-receivers COMMAND nsc_readonly_receiver_tests)

    add_executable(nsc_generic_alias_tests tests/unit/GenericAliasTests.cpp)
    target_link_libraries(nsc_generic_alias_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_generic_alias_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_generic_alias_tests)
    add_test(NAME nsc.language.generic-aliases COMMAND nsc_generic_alias_tests)

    add_executable(nsc_generic_inference_tests tests/unit/GenericInferenceTests.cpp)
    target_link_libraries(nsc_generic_inference_tests PRIVATE nsc_frontend nsc_compilation)
    target_compile_features(nsc_generic_inference_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_generic_inference_tests)
    add_test(NAME nsc.language.generic-inference COMMAND nsc_generic_inference_tests)

    add_executable(nsc_inlining_policy_tests tests/unit/InliningPolicyTests.cpp)
    target_link_libraries(nsc_inlining_policy_tests PRIVATE nsc_frontend)
    target_compile_features(nsc_inlining_policy_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_inlining_policy_tests)
    add_test(NAME nsc.language.inlining-policy COMMAND nsc_inlining_policy_tests)

    add_executable(nsc_inlining_modifier_tests tests/unit/InliningModifierTests.cpp)
    target_link_libraries(nsc_inlining_modifier_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_inlining_modifier_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_inlining_modifier_tests)
    add_test(NAME nsc.language.inline-modifier COMMAND nsc_inlining_modifier_tests)

    add_executable(nsc_forceinline_modifier_tests tests/unit/ForceInlineModifierTests.cpp)
    target_link_libraries(nsc_forceinline_modifier_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_forceinline_modifier_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_forceinline_modifier_tests)
    add_test(NAME nsc.language.forceinline-modifier COMMAND nsc_forceinline_modifier_tests)

    add_executable(nsc_noinline_modifier_tests tests/unit/NoInlineModifierTests.cpp)
    target_link_libraries(nsc_noinline_modifier_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_noinline_modifier_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_noinline_modifier_tests)
    add_test(NAME nsc.language.noinline-modifier COMMAND nsc_noinline_modifier_tests)

    add_executable(nsc_generic_inlining_tests tests/unit/GenericInliningTests.cpp)
    target_link_libraries(nsc_generic_inlining_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_generic_inlining_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_generic_inlining_tests)
    add_test(NAME nsc.language.generic-inlining COMMAND nsc_generic_inlining_tests)

    add_executable(nsc_struct_interface_inlining_tests tests/unit/StructInterfaceInliningTests.cpp)
    target_link_libraries(nsc_struct_interface_inlining_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_struct_interface_inlining_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_struct_interface_inlining_tests)
    add_test(NAME nsc.language.struct-interface-inlining COMMAND nsc_struct_interface_inlining_tests)

    add_executable(nsc_cross_library_inlining_tests tests/unit/CrossLibraryInliningTests.cpp)
    target_link_libraries(nsc_cross_library_inlining_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_cross_library_inlining_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_cross_library_inlining_tests)
    add_test(NAME nsc.language.cross-library-inlining COMMAND nsc_cross_library_inlining_tests)

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

    add_executable(nsc_stage67_e11_lambda_execution_tests tests/unit/Stage67E11LambdaExecutionTests.cpp)
    target_link_libraries(nsc_stage67_e11_lambda_execution_tests PRIVATE
        nsc_driver nsc_linker nsc_llvm nsc_frontend nsc_target nsc_runtime)
    target_compile_features(nsc_stage67_e11_lambda_execution_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage67_e11_lambda_execution_tests)
    add_test(NAME nsc.stage67.e11-lambda-execution COMMAND nsc_stage67_e11_lambda_execution_tests)

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

    # Entrega 228 -- primera familia SIMD publica, inspeccionada en LLVM a O0.
    add_executable(nsc_stage68_simd_tests tests/unit/Stage68SimdTests.cpp)
    target_link_libraries(nsc_stage68_simd_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage68_simd_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage68_simd_tests)
    add_test(NAME nsc.stage68.simd COMMAND nsc_stage68_simd_tests)

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

    # Stage 60 / 1.0-E7 -- normative language contract. This does not pretend that prose proves
    # semantics. It verifies the traceability property E7 adds: every normative clause has one
    # implementation location and one executable evidence location, every referenced file exists,
    # the identifiers are unique in both directions, and the installed specification is wired into
    # the package. The existing focused suites remain the semantic evidence named by the map.
    add_executable(nsc_stage60_language_contract_tests tests/unit/Stage60LanguageContractTests.cpp)
    target_compile_features(nsc_stage60_language_contract_tests PRIVATE cxx_std_20)
    target_include_directories(nsc_stage60_language_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(nsc_stage60_language_contract_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage60_language_contract_tests)
    add_test(NAME nsc.stage60.language-contract COMMAND nsc_stage60_language_contract_tests)

    # Stage 61 / 1.0-E8 -- compiler hardening. Exercises the diagnostic budget, executable crash
    # boundary, the loaded LLVM 23.x gate, its public inspection command, and the traceability of
    # the compiler contract. This suite links the real driver and LLVM runtime on purpose.
    add_executable(nsc_stage61_compiler_hardening_tests tests/unit/Stage61CompilerHardeningTests.cpp)
    target_link_libraries(
        nsc_stage61_compiler_hardening_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage61_compiler_hardening_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        nsc_stage61_compiler_hardening_tests
        PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage61_compiler_hardening_tests)
    add_test(NAME nsc.stage61.compiler-hardening COMMAND nsc_stage61_compiler_hardening_tests)

    # Stage 62 / 1.0-E9 -- contrato reproducible del toolchain. Prueba el formato canonico, su
    # escritura e instalacion, el verificador estricto y las tres superficies publicas del driver.
    add_executable(nsc_stage62_toolchain_contract_tests tests/unit/Stage62ToolchainContractTests.cpp)
    target_link_libraries(
        nsc_stage62_toolchain_contract_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage62_toolchain_contract_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        nsc_stage62_toolchain_contract_tests
        PRIVATE
            NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            NSC_TOOLCHAIN_CONTRACT_PATH="${NSC_TOOLCHAIN_CONTRACT}")
    add_dependencies(nsc_stage62_toolchain_contract_tests nsc)
    nsc_enable_warnings(nsc_stage62_toolchain_contract_tests)
    add_test(NAME nsc.stage62.toolchain-contract COMMAND nsc_stage62_toolchain_contract_tests)

    # Stage 63 / 1.0-E9 -- la instalacion se valida como producto completo. Los casos negativos
    # prueban cada familia del layout y el comando publico que ejecuta CMake al final del install.
    add_executable(nsc_stage63_installation_verifier_tests tests/unit/Stage63InstallationVerifierTests.cpp)
    target_link_libraries(
        nsc_stage63_installation_verifier_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage63_installation_verifier_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        nsc_stage63_installation_verifier_tests
        PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage63_installation_verifier_tests)
    add_test(NAME nsc.stage63.installation-verifier COMMAND nsc_stage63_installation_verifier_tests)

    # Stage 64 / 1.0-E9 -- inventario criptografico de la instalacion. Comparte la autoridad
    # SHA-256 usada por NSMETA, prueba el formato determinista y demuestra rechazo de bytes
    # alterados, archivos faltantes, residuos inesperados y entradas malformadas.
    add_executable(
        nsc_stage64_installation_manifest_tests
        tests/unit/Stage64InstallationManifestTests.cpp)
    target_link_libraries(
        nsc_stage64_installation_manifest_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage64_installation_manifest_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        nsc_stage64_installation_manifest_tests
        PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage64_installation_manifest_tests)
    add_test(
        NAME nsc.stage64.installation-manifest
        COMMAND nsc_stage64_installation_manifest_tests)

    # Stage 66 / megaentrega unica E9+E10. Stage 65 queda absorbido en esta fotografia final:
    # compara dos instalaciones, produce planes autenticados de actualizacion/retiro y congela
    # la politica de compatibilidad publica en un artefacto instalado y verificable.
    add_executable(
        nsc_stage66_e9_e10_release_contract_tests
        tests/unit/Stage66E9E10ReleaseContractTests.cpp)
    target_link_libraries(
        nsc_stage66_e9_e10_release_contract_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(
        nsc_stage66_e9_e10_release_contract_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        nsc_stage66_e9_e10_release_contract_tests
        PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage66_e9_e10_release_contract_tests)
    add_test(
        NAME nsc.stage66.e9-e10-release-contract
        COMMAND nsc_stage66_e9_e10_release_contract_tests)

    # Delivery 234 -- a canonical 1.x contract snapshot makes every future 2.0
    # break explicit and machine-checkable before implementation or promotion.
    add_executable(
        nsc_stage69_migration_baseline_tests
        tests/unit/Stage69MigrationBaselineTests.cpp)
    target_link_libraries(
        nsc_stage69_migration_baseline_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage69_migration_baseline_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage69_migration_baseline_tests)
    add_test(
        NAME nsc.stage69.migration-baseline
        COMMAND nsc_stage69_migration_baseline_tests)

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

    add_executable(nsc_stage48_2_external_template_type_qualification_tests
        tests/unit/Stage48_2ExternalTemplateTypeQualificationTests.cpp)
    target_link_libraries(nsc_stage48_2_external_template_type_qualification_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_2_external_template_type_qualification_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_2_external_template_type_qualification_tests)
    add_test(
        NAME nsc.stage48-2.external-template-type-qualification
        COMMAND nsc_stage48_2_external_template_type_qualification_tests)

    add_executable(nsc_stage48_2_cross_module_callable_identity_tests
        tests/unit/Stage48_2CrossModuleCallableIdentityTests.cpp)
    target_link_libraries(nsc_stage48_2_cross_module_callable_identity_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_2_cross_module_callable_identity_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage48_2_cross_module_callable_identity_tests)
    add_test(
        NAME nsc.stage48-2.cross-module-callable-identity
        COMMAND nsc_stage48_2_cross_module_callable_identity_tests)

    add_executable(nsc_stage46_1_dll_cross_linker_matrix_tests
        tests/unit/Stage46_1DllCrossLinkerMatrixTests.cpp)
    target_link_libraries(nsc_stage46_1_dll_cross_linker_matrix_tests
        PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage46_1_dll_cross_linker_matrix_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_stage46_1_dll_cross_linker_matrix_tests)
    add_test(NAME nsc.stage46-1.dll-cross-linker-matrix
        COMMAND nsc_stage46_1_dll_cross_linker_matrix_tests)

    # Host-contract tests launch the GNU-style Clang drivers, not the clang-cl driver used to
    # build nsc itself. Resolve them at configure time so the hermetic baseline can clear PATH
    # without turning ABI tests into environment-dependent command-name lookups.
    find_program(NSC_TEST_CLANG_EXECUTABLE
        NAMES clang clang.exe
        HINTS "${LLVM_TOOLS_BINARY_DIR}" "C:/clang-llvm/bin"
        REQUIRED)
    find_program(NSC_TEST_CLANGXX_EXECUTABLE
        NAMES clang++ clang++.exe
        HINTS "${LLVM_TOOLS_BINARY_DIR}" "C:/clang-llvm/bin"
        REQUIRED)

    add_executable(nsc_stage47_host_c_abi_tests tests/unit/Stage47HostCAbiTests.cpp)
    target_link_libraries(nsc_stage47_host_c_abi_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage47_host_c_abi_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage47_host_c_abi_tests PRIVATE
        NSC_TEST_CLANG_EXECUTABLE="${NSC_TEST_CLANG_EXECUTABLE}"
        NSC_TEST_CLANGXX_EXECUTABLE="${NSC_TEST_CLANGXX_EXECUTABLE}")
    nsc_enable_warnings(nsc_stage47_host_c_abi_tests)
    add_test(NAME nsc.stage47.host-c-abi COMMAND nsc_stage47_host_c_abi_tests)

    add_executable(nsc_stage48_1_c_header_tests tests/unit/Stage48_1CHeaderGeneratorTests.cpp)
    target_link_libraries(nsc_stage48_1_c_header_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_1_c_header_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_1_c_header_tests PRIVATE
        NSC_TEST_CLANG_EXECUTABLE="${NSC_TEST_CLANG_EXECUTABLE}"
        NSC_TEST_CLANGXX_EXECUTABLE="${NSC_TEST_CLANGXX_EXECUTABLE}")
    nsc_enable_warnings(nsc_stage48_1_c_header_tests)
    add_test(NAME nsc.stage48_1.c-header COMMAND nsc_stage48_1_c_header_tests)

    add_executable(nsc_stage48_2_library_metadata_tests tests/unit/Stage48_2LibraryMetadataTests.cpp)
    target_link_libraries(nsc_stage48_2_library_metadata_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_2_library_metadata_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_2_library_metadata_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_stage48_2_library_metadata_tests)
    add_test(NAME nsc.stage48_2.library-metadata COMMAND nsc_stage48_2_library_metadata_tests)

    add_executable(nsc_library_metadata_merger_tests tests/unit/LibraryMetadataMergerTests.cpp)
    target_link_libraries(nsc_library_metadata_merger_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_library_metadata_merger_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_library_metadata_merger_tests)
    add_test(NAME nsc.library-metadata-merger COMMAND nsc_library_metadata_merger_tests)

    add_executable(nsc_stage48_3_c_host_tests tests/unit/Stage48_3CHostIntegrationTests.cpp)
    target_link_libraries(nsc_stage48_3_c_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_3_c_host_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_3_c_host_tests PRIVATE
        NSC_TEST_CLANG_EXECUTABLE="${NSC_TEST_CLANG_EXECUTABLE}")
    nsc_enable_warnings(nsc_stage48_3_c_host_tests)
    add_test(NAME nsc.stage48_3.c-host COMMAND nsc_stage48_3_c_host_tests)

    add_executable(nsc_stage48_4_cpp_host_tests tests/unit/Stage48_4CppHostIntegrationTests.cpp)
    target_link_libraries(nsc_stage48_4_cpp_host_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_stage48_4_cpp_host_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_stage48_4_cpp_host_tests PRIVATE
        NSC_TEST_CLANGXX_EXECUTABLE="${NSC_TEST_CLANGXX_EXECUTABLE}")
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
    target_compile_definitions(nsc_stage48_6_cppcli_host_tests PRIVATE
        NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NSC_TEST_CLANGXX_EXECUTABLE="${NSC_TEST_CLANGXX_EXECUTABLE}")
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

    # NativeSharp 2.0 item 308: field-level volatile memory for MMIO. This focused O0 test
    # deliberately proves that volatile accesses are emitted without acquiring atomic/fence
    # semantics; synchronization remains the responsibility of the Stage 54.2 primitives.
    add_executable(nsc_item308_volatile_mmio_tests tests/unit/Item308VolatileMmioTests.cpp)
    target_link_libraries(nsc_item308_volatile_mmio_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item308_volatile_mmio_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item308_volatile_mmio_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item308_volatile_mmio_tests)
    add_test(NAME nsc.item308.volatile-mmio COMMAND nsc_item308_volatile_mmio_tests)

    # Stage 54.3 "Mutex" -- same extra rigor as Stage 54.1/54.2, PLUS dedicated regression coverage

    # NativeSharp 2.0 item 309: the existing AtomicInt64 implementation predates this roadmap.
    # This focused O0 target certifies full-width boundaries, native i64 instructions, alignment,
    # return contracts, and the absence of an external libatomic fallback.
    add_executable(nsc_item309_atomic_int64_tests tests/unit/Item309AtomicInt64Tests.cpp)
    target_link_libraries(nsc_item309_atomic_int64_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item309_atomic_int64_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item309_atomic_int64_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item309_atomic_int64_tests)
    add_test(NAME nsc.item309.atomic-int64 COMMAND nsc_item309_atomic_int64_tests)

    # NativeSharp 2.0 item 310: native pointer-sized atomics over non-owning void* values.
    add_executable(nsc_item310_atomic_pointer_tests tests/unit/Item310AtomicPointerTests.cpp)
    target_link_libraries(nsc_item310_atomic_pointer_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item310_atomic_pointer_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item310_atomic_pointer_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item310_atomic_pointer_tests)
    add_test(NAME nsc.item310.atomic-pointer COMMAND nsc_item310_atomic_pointer_tests)

    # NativeSharp 2.0 item 311: public static Interlocked operations over true ref locations.
    add_executable(nsc_item311_interlocked_api_tests tests/unit/Item311InterlockedApiTests.cpp)
    target_link_libraries(nsc_item311_interlocked_api_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item311_interlocked_api_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item311_interlocked_api_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item311_interlocked_api_tests)
    add_test(NAME nsc.item311.interlocked-api COMMAND nsc_item311_interlocked_api_tests)

    # NativeSharp 2.0 item 312: exact-type mutable aliases with reference-local storage.
    add_executable(nsc_item312_reference_locals_tests tests/unit/Item312ReferenceLocalsTests.cpp)
    target_link_libraries(nsc_item312_reference_locals_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item312_reference_locals_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item312_reference_locals_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item312_reference_locals_tests)
    add_test(NAME nsc.item312.reference-locals COMMAND nsc_item312_reference_locals_tests)

    # NativeSharp 2.0 item 313: address-preserving ref returns and ref-return call lvalues.
    add_executable(nsc_item313_reference_returns_tests tests/unit/Item313ReferenceReturnsTests.cpp)
    target_link_libraries(nsc_item313_reference_returns_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item313_reference_returns_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item313_reference_returns_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item313_reference_returns_tests)
    add_test(NAME nsc.item313.reference-returns COMMAND nsc_item313_reference_returns_tests)

    # NativeSharp 2.0 item 314: get-only ref indexers preserve address identity through direct,
    # virtual, interface, and precompiled-library calls.
    add_executable(nsc_item314_reference_indexers_tests tests/unit/Item314ReferenceIndexersTests.cpp)
    target_link_libraries(nsc_item314_reference_indexers_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item314_reference_indexers_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item314_reference_indexers_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item314_reference_indexers_tests)
    add_test(NAME nsc.item314.reference-indexers COMMAND nsc_item314_reference_indexers_tests)

    # NativeSharp 2.0 item 315: one reference-origin analysis closes transitive frame escapes
    # through aliases, value aggregates, index expressions, and ref-return forwarding calls.
    add_executable(nsc_item315_unified_escape_analysis_tests tests/unit/Item315UnifiedEscapeAnalysisTests.cpp)
    target_link_libraries(nsc_item315_unified_escape_analysis_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item315_unified_escape_analysis_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item315_unified_escape_analysis_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item315_unified_escape_analysis_tests)
    add_test(NAME nsc.item315.unified-escape-analysis COMMAND nsc_item315_unified_escape_analysis_tests)

    # NativeSharp 2.0 item 316: reference aliases retain a maximum valid lexical region and every
    # declaration/return is checked against it.
    add_executable(nsc_item316_reference_lifetime_analysis_tests tests/unit/Item316ReferenceLifetimeAnalysisTests.cpp)
    target_link_libraries(nsc_item316_reference_lifetime_analysis_tests PRIVATE nsc_driver)
    target_compile_features(nsc_item316_reference_lifetime_analysis_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_item316_reference_lifetime_analysis_tests PRIVATE
        NSC_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    nsc_enable_warnings(nsc_item316_reference_lifetime_analysis_tests)
    add_test(NAME nsc.item316.reference-lifetime-analysis COMMAND nsc_item316_reference_lifetime_analysis_tests)
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
        nsc_item394_stage_boundary_verifier_tests
        tests/unit/Item394StageBoundaryVerifierTests.cpp
        nsc.item394.stage-boundary-verifiers)
    add_executable(nsc_item395_mir_layout_abi_tests tests/unit/Item395MirLayoutAbiTests.cpp)
    target_link_libraries(nsc_item395_mir_layout_abi_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item395_mir_layout_abi_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item395_mir_layout_abi_tests)
    add_test(NAME nsc.item395.mir-layout-abi COMMAND nsc_item395_mir_layout_abi_tests)
    add_executable(nsc_item396_mir_ownership_cleanup_tests tests/unit/Item396MirOwnershipCleanupTests.cpp)
    target_link_libraries(nsc_item396_mir_ownership_cleanup_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item396_mir_ownership_cleanup_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item396_mir_ownership_cleanup_tests)
    add_test(NAME nsc.item396.mir-ownership-cleanup COMMAND nsc_item396_mir_ownership_cleanup_tests)
    add_executable(nsc_item397_mir_exception_flow_tests tests/unit/Item397MirExceptionFlowTests.cpp)
    target_link_libraries(nsc_item397_mir_exception_flow_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item397_mir_exception_flow_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item397_mir_exception_flow_tests)
    add_test(NAME nsc.item397.mir-exception-flow COMMAND nsc_item397_mir_exception_flow_tests)
    add_executable(nsc_item398_mir_generics_interfaces_tests tests/unit/Item398MirGenericsInterfacesTests.cpp)
    target_link_libraries(nsc_item398_mir_generics_interfaces_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item398_mir_generics_interfaces_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item398_mir_generics_interfaces_tests)
    add_test(NAME nsc.item398.mir-generics-interfaces COMMAND nsc_item398_mir_generics_interfaces_tests)
    add_executable(nsc_item399_llvm_backend_encapsulation_tests tests/unit/Item399LlvmBackendEncapsulationTests.cpp)
    target_link_libraries(nsc_item399_llvm_backend_encapsulation_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item399_llvm_backend_encapsulation_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item399_llvm_backend_encapsulation_tests)
    add_test(NAME nsc.item399.llvm-backend-encapsulation COMMAND nsc_item399_llvm_backend_encapsulation_tests)
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

    add_executable(nsc_compilation_artifact_cache_tests tests/unit/CompilationArtifactCacheTests.cpp)
    target_link_libraries(nsc_compilation_artifact_cache_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_compilation_artifact_cache_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_compilation_artifact_cache_tests)
    add_test(NAME nsc.compilation-artifact-cache COMMAND nsc_compilation_artifact_cache_tests)

    add_executable(nsc_compilation_artifact_cache_driver_tests tests/unit/CompilationArtifactCacheDriverTests.cpp)
    target_link_libraries(nsc_compilation_artifact_cache_driver_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_compilation_artifact_cache_driver_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_compilation_artifact_cache_driver_tests)
    add_test(NAME nsc.compilation-artifact-cache-driver COMMAND nsc_compilation_artifact_cache_driver_tests)

    add_executable(nsc_compilation_profile_tests tests/unit/CompilationProfileTests.cpp)
    target_link_libraries(nsc_compilation_profile_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_compilation_profile_tests PRIVATE cxx_std_20)
    target_compile_definitions(nsc_compilation_profile_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nsc_enable_warnings(nsc_compilation_profile_tests)
    add_test(NAME nsc.compilation-profile COMMAND nsc_compilation_profile_tests)

    add_executable(nsc_llvm_optimization_tests tests/unit/LlvmOptimizationTests.cpp)
    target_link_libraries(nsc_llvm_optimization_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_llvm_optimization_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_llvm_optimization_tests)
    add_test(NAME nsc.llvm-optimization COMMAND nsc_llvm_optimization_tests)

    add_executable(nsc_item389_native_sanitizer_tests tests/unit/Item389NativeSanitizerTests.cpp)
    target_link_libraries(nsc_item389_native_sanitizer_tests PRIVATE nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item389_native_sanitizer_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item389_native_sanitizer_tests)
    add_test(NAME nsc.item389-native-sanitizer COMMAND nsc_item389_native_sanitizer_tests)

    add_executable(nsc_item390_lto_tests tests/unit/Item390LtoTests.cpp)
    target_link_libraries(nsc_item390_lto_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item390_lto_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item390_lto_tests)
    add_test(NAME nsc.item390-lto COMMAND nsc_item390_lto_tests)

    add_executable(nsc_item391_thinlto_tests tests/unit/Item391ThinLtoTests.cpp)
    target_link_libraries(nsc_item391_thinlto_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item391_thinlto_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item391_thinlto_tests)
    add_test(NAME nsc.item391-thinlto COMMAND nsc_item391_thinlto_tests)

    add_executable(nsc_item392_incremental_link_tests tests/unit/Item392IncrementalLinkTests.cpp)
    target_link_libraries(nsc_item392_incremental_link_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item392_incremental_link_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item392_incremental_link_tests)
    add_test(NAME nsc.item392-incremental-link COMMAND nsc_item392_incremental_link_tests)

    add_executable(nsc_item393_compatibility_matrix_tests tests/unit/Item393CompatibilityMatrixTests.cpp)
    target_link_libraries(nsc_item393_compatibility_matrix_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item393_compatibility_matrix_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item393_compatibility_matrix_tests)
    add_test(NAME nsc.item393-compatibility-matrix COMMAND nsc_item393_compatibility_matrix_tests)

    add_executable(nsc_item402_breaking_change_ledger_tests tests/unit/Item402BreakingChangeLedgerTests.cpp)
    target_link_libraries(nsc_item402_breaking_change_ledger_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item402_breaking_change_ledger_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item402_breaking_change_ledger_tests)
    add_test(NAME nsc.item402.breaking-change-ledger COMMAND nsc_item402_breaking_change_ledger_tests)

    add_executable(nsc_item403_migration_guide_tests tests/unit/Item403MigrationGuideTests.cpp)
    target_link_libraries(nsc_item403_migration_guide_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item403_migration_guide_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item403_migration_guide_tests)
    add_test(NAME nsc.item403.migration-guide COMMAND nsc_item403_migration_guide_tests)

    add_executable(nsc_item404_release_candidate_freeze_tests tests/unit/Item404ReleaseCandidateFreezeTests.cpp)
    target_link_libraries(nsc_item404_release_candidate_freeze_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item404_release_candidate_freeze_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item404_release_candidate_freeze_tests)
    add_test(NAME nsc.item404.release-candidate-freeze COMMAND nsc_item404_release_candidate_freeze_tests)

    add_executable(nsc_item405_final_compatibility_tests tests/unit/Item405FinalCompatibilityCertificationTests.cpp)
    target_link_libraries(nsc_item405_final_compatibility_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item405_final_compatibility_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item405_final_compatibility_tests)
    add_test(NAME nsc.item405.final-compatibility COMMAND nsc_item405_final_compatibility_tests)

    add_executable(nsc_item406_final_baseline_contract_tests tests/unit/Item406FinalBaselineContractTests.cpp)
    target_link_libraries(nsc_item406_final_baseline_contract_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item406_final_baseline_contract_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item406_final_baseline_contract_tests)
    add_test(NAME nsc.item406.final-baseline-contract COMMAND nsc_item406_final_baseline_contract_tests)

    add_executable(nsc_item407_official_promotion_tests tests/unit/Item407OfficialPromotionTests.cpp)
    target_link_libraries(nsc_item407_official_promotion_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item407_official_promotion_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item407_official_promotion_tests)
    add_test(NAME nsc.item407.official-promotion COMMAND nsc_item407_official_promotion_tests)

    add_executable(nsc_windows_runtime_tests tests/unit/WindowsRuntimeTests.cpp)
    target_link_libraries(nsc_windows_runtime_tests PRIVATE nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_windows_runtime_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_windows_runtime_tests)
    add_test(NAME nsc.windows-runtime COMMAND nsc_windows_runtime_tests)

    add_executable(nsc_item400_runtime_abi_contract_tests tests/unit/Item400RuntimeAbiContractTests.cpp)
    target_link_libraries(nsc_item400_runtime_abi_contract_tests PRIVATE nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item400_runtime_abi_contract_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item400_runtime_abi_contract_tests)
    add_test(NAME nsc.item400.runtime-abi-contract COMMAND nsc_item400_runtime_abi_contract_tests)

    add_executable(nsc_item401_independent_windows_runtime_tests tests/unit/Item401IndependentWindowsRuntimeTests.cpp)
    target_link_libraries(nsc_item401_independent_windows_runtime_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
    target_compile_features(nsc_item401_independent_windows_runtime_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_item401_independent_windows_runtime_tests)
    add_test(NAME nsc.item401.independent-windows-runtime COMMAND nsc_item401_independent_windows_runtime_tests)

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

    add_executable(nsc_compilation_worker_pool_tests tests/unit/CompilationWorkerPoolTests.cpp)
    target_link_libraries(nsc_compilation_worker_pool_tests PRIVATE nsc_driver)
    target_compile_features(nsc_compilation_worker_pool_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_compilation_worker_pool_tests)
    add_test(NAME nsc.compilation-worker-pool COMMAND nsc_compilation_worker_pool_tests)

    add_executable(nsc_compilation_graph_scheduler_tests tests/unit/CompilationGraphSchedulerTests.cpp)
    target_link_libraries(nsc_compilation_graph_scheduler_tests PRIVATE nsc_driver)
    target_compile_features(nsc_compilation_graph_scheduler_tests PRIVATE cxx_std_20)
    nsc_enable_warnings(nsc_compilation_graph_scheduler_tests)
    add_test(NAME nsc.compilation-graph-scheduler COMMAND nsc_compilation_graph_scheduler_tests)

    find_program(NSC_LLVM_PDBUTIL_EXECUTABLE
        NAMES llvm-pdbutil
        HINTS "${LLVM_TOOLS_BINARY_DIR}" "C:/clang-llvm/bin"
        REQUIRED)
    find_program(NSC_LLVM_READOBJ_EXECUTABLE
        NAMES llvm-readobj
        HINTS "${LLVM_TOOLS_BINARY_DIR}" "C:/clang-llvm/bin"
        REQUIRED)
    find_program(NSC_DEBUG_LLD_LINK_EXECUTABLE
        NAMES lld-link lld-link.exe
        HINTS "${LLVM_TOOLS_BINARY_DIR}" "C:/clang-llvm/bin"
        REQUIRED)
    add_test(
        NAME nsc.debug.codeview-pdb
        COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_LLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DLLVM_PDBUTIL=${NSC_LLVM_PDBUTIL_EXECUTABLE}"
            "-DLLVM_READOBJ=${NSC_LLVM_READOBJ_EXECUTABLE}"
            "-DDEBUG_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/debug_pdb/DebugPdb.nsproj"
            "-DDEBUG_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/debug-artifact-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpDebugArtifacts.cmake")

    add_test(
        NAME nsc.debug.extreme-o0
        COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_LLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DLLVM_PDBUTIL=${NSC_LLVM_PDBUTIL_EXECUTABLE}"
            "-DLLVM_READOBJ=${NSC_LLVM_READOBJ_EXECUTABLE}"
            "-DDEBUG_EXTREME_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/debug_pdb_extreme"
            "-DDEBUG_CORE_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/lib/NativeSharp.Core/NativeSharp.Core.nsproj"
            "-DDEBUG_DIAGNOSTICS_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/lib/NativeSharp.Core.Diagnostics/NativeSharp.Core.Diagnostics.nsproj"
            "-DDEBUG_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/debug-extreme-o0-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpExtremeDebugO0.cmake")

    file(GLOB _nsc_dia_sdk_roots
        "C:/Program Files/Microsoft Visual Studio/2022/*/DIA SDK")
    find_path(NSC_DIA_INCLUDE_DIR dia2.h
        HINTS ${_nsc_dia_sdk_roots}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH)
    find_file(NSC_DIA_GUIDS_LIBRARY diaguids.lib
        HINTS ${_nsc_dia_sdk_roots}
        PATH_SUFFIXES lib/amd64
        NO_DEFAULT_PATH)
    find_file(NSC_DIA_RUNTIME msdia140.dll
        HINTS ${_nsc_dia_sdk_roots}
        PATH_SUFFIXES bin/amd64
        NO_DEFAULT_PATH)
    if(NSC_DIA_INCLUDE_DIR AND NSC_DIA_GUIDS_LIBRARY AND NSC_DIA_RUNTIME)
        add_executable(nsc_debugger_consumer_handoff
            tests/integration/debugger_consumers/DebuggerConsumerHandoff.cpp)
        target_compile_features(nsc_debugger_consumer_handoff PRIVATE cxx_std_20)
        target_include_directories(nsc_debugger_consumer_handoff PRIVATE "${NSC_DIA_INCLUDE_DIR}")
        target_link_libraries(nsc_debugger_consumer_handoff PRIVATE
            "${NSC_DIA_GUIDS_LIBRARY}" ole32 dbghelp)
        add_test(
            NAME nsc.debug.consumer-handoff
            COMMAND nsc_debugger_consumer_handoff
                "${NSC_DIA_RUNTIME}"
                "${CMAKE_CURRENT_BINARY_DIR}/debug-extreme-o0-test/Consumer/DebugExtremeConsumer.exe"
                "${CMAKE_CURRENT_BINARY_DIR}/debug-extreme-o0-test/Consumer/DebugExtremeConsumer.pdb")
        set_tests_properties(nsc.debug.consumer-handoff PROPERTIES DEPENDS nsc.debug.extreme-o0)
    else()
        message(STATUS "DIA SDK not found; Visual Studio consumer handoff test is unavailable")
    endif()

    add_test(
        NAME nsc.debug.inline-stepping-o0
        COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_LLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DLLVM_PDBUTIL=${NSC_LLVM_PDBUTIL_EXECUTABLE}"
            "-DINLINE_DEBUG_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/inline_debug/InlineDebug.nsproj"
            "-DINLINE_DEBUG_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/inline-debug-o0-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpInlineDebugO0.cmake")

    add_test(
        NAME nsc.debug.assembly-stepping-o0
        COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_LLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DLLVM_PDBUTIL=${NSC_LLVM_PDBUTIL_EXECUTABLE}"
            "-DASSEMBLY_DEBUG_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/asm_debug/AssemblyDebug.nsproj"
            "-DASSEMBLY_DEBUG_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/assembly-debug-o0-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpAssemblyDebugO0.cmake")

    add_test(
        NAME nsc.assembly.abi-c-cpp-o0
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DCLANG_C=${NSC_TEST_CLANG_EXECUTABLE}"
            "-DCLANG_CXX=${NSC_TEST_CLANGXX_EXECUTABLE}"
            "-DLLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "-DASSEMBLY_ABI_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/asm_abi"
            "-DASSEMBLY_ABI_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/assembly-abi-o0-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpAssemblyAbiO0.cmake")
    # Windows occasionally reports an incorrect child exit status for this freestanding host
    # while many process-heavy CTests run concurrently. The product itself remains correct and
    # repeated isolated runs are stable, so keep the strict 42 check and isolate only this test.
    set_tests_properties(nsc.assembly.abi-c-cpp-o0 PROPERTIES RUN_SERIAL TRUE)

    add_test(
        NAME nsc.item333.win64-vararg-abi
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
            "-DCLANG_C=${NSC_TEST_CLANG_EXECUTABLE}"
            "-DCLANG_CXX=${NSC_TEST_CLANGXX_EXECUTABLE}"
            "-DLLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"
            "-DVARARG_ABI_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/item333_vararg_abi"
            "-DVARARG_ABI_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/item333-vararg-abi-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpWin64VarargAbi.cmake")

    # CC1-000: este regex comprueba la identidad visible del compilador instalado. El stage exacto
    # se extrae de Version.h para conservar una sola fuente de verdad sin aceptar una instalacion
    # atrasada. nsc.version.identity comprueba por separado las constantes enlazadas en este build.
    #
    # `[0-9a-f]` y no `.*` a proposito: un @NSC_BUILD_ID@ sin sustituir dejaria la linea vacia, y un
    # `.*` la aceptaria igual.
    file(
        STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/nativesharp/Basic/Version.h"
        NSC_COMPILER_STAGE_DECLARATION
        REGEX "kCompilerStage = \"[0-9]+\"")
    string(
        REGEX REPLACE ".*\"([0-9]+)\".*" "\\1"
        NSC_EXPECTED_COMPILER_STAGE "${NSC_COMPILER_STAGE_DECLARATION}")
    if(NOT NSC_EXPECTED_COMPILER_STAGE MATCHES "^[0-9]+$")
        message(FATAL_ERROR "No se pudo leer kCompilerStage desde Version.h")
    endif()

    add_test(NAME nsc.version COMMAND "${NSC_TEST_COMPILER}" --version)
    set_tests_properties(
        nsc.version
        PROPERTIES PASS_REGULAR_EXPRESSION
            "nsc 2.0.0[+]build[.][0-9TZ]+[.][0-9a-f]+.*NativeSharp 2.0.0.*stage ${NSC_EXPECTED_COMPILER_STAGE}.*build [0-9a-f][0-9a-f]+")

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
        PROPERTIES PASS_REGULAR_EXPRESSION "abi=win64-direct")

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
    # --print-linker describes the selected PE/COFF toolchain. The executable may be lld-link or
    # NSLink, so the integration contract must validate the backend flavor instead of one provider.
    set_tests_properties(
        nsc.linker-info
        PROPERTIES PASS_REGULAR_EXPRESSION "flavor: Windows COFF/PE")

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
