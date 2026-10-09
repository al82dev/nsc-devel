cmake_minimum_required(VERSION 3.25)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(NativeSharpCompilerSuiteInfrastructure LANGUAGES CXX)
    enable_testing()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(NSC_COMPILER_SUITE_NSC_EXECUTABLE "" CACHE FILEPATH "Explicit A:-drive NSC for process-owned suite cases")
set(NSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE "" CACHE FILEPATH "Second explicit NSC for build-identity comparison")
set(NSC_COMPILER_SUITE_BUILD_ID_HEADER "" CACHE FILEPATH "Generated BuildId.h owned by the selected NSC")

add_executable(
    nsc_component_tests
    host/src/main.cpp
    host/src/TestRegistry.cpp
    host/modules/InfrastructureModule.cpp
    host/modules/BasicValuesModule.cpp
    host/modules/BasicSha256Module.cpp
    host/modules/BasicSha256FileModule.cpp
    host/modules/BasicOutputFailureModule.cpp
    host/modules/BasicArtifactStagingModule.cpp
    host/modules/BasicArtifactPreflightModule.cpp
    host/modules/BasicArtifactControlledPreflightModule.cpp
    host/modules/BasicTransactionalWriterModule.cpp
    host/modules/BasicArtifactSeedModule.cpp
    host/modules/SourcePositionModule.cpp
    host/modules/SourceFileOwnershipModule.cpp
    host/modules/SourceLineIndexModule.cpp
    host/modules/SourceOffsetPositionModule.cpp
    host/modules/SourceLineTextModule.cpp
    host/modules/SourceFileReadModule.cpp
    host/modules/SourceManagerRegistryModule.cpp
    host/modules/SourceManagerQueryModule.cpp
    host/modules/TargetDomainEnumsModule.cpp
    host/modules/TargetLayoutValuesModule.cpp
    host/modules/TargetAggregateRequestModule.cpp
    host/modules/TargetWindowsFactoriesModule.cpp
    host/modules/TargetBuiltinLayoutModule.cpp
    host/modules/TargetCheckedArithmeticModule.cpp
    host/modules/TargetPowerOfTwoAlignmentModule.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Basic/Sha256.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Basic/ArtifactTransaction.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Source/SourceFile.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Source/SourceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Target/TargetDataLayout.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Target/TargetInfo.cpp")

target_include_directories(
    nsc_component_tests
    PRIVATE
        host/include
        "${CMAKE_CURRENT_LIST_DIR}/../../include")
target_compile_features(nsc_component_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(nsc_component_tests PRIVATE /W4 /WX)
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME nsc.compiler-suite.infrastructure COMMAND nsc_component_tests --all)
add_test(NAME nsc.compiler-suite.host-basic-values COMMAND nsc_component_tests --component Host.Basic)
add_test(
    NAME nsc.compiler-suite.controller-infrastructure
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_infrastructure_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/controller-gate")
add_test(
    NAME nsc.compiler-suite.host-basic-values-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_values_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-values-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-basic-sha256-file-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_sha256_file_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-sha256-file-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-basic-artifact-staging-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_artifact_staging_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-artifact-staging-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-basic-artifact-preflight-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_artifact_preflight_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-artifact-preflight-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-basic-transactional-writer-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_transactional_writer_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-transactional-writer-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-basic-artifact-seed-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_artifact_seed_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-artifact-seed-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-position-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_position_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-position-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-file-ownership-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_file_ownership_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-file-ownership-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-line-index-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_line_index_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-line-index-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-offset-position-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_offset_position_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-offset-position-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-line-text-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_line_text_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-line-text-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-file-read-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_file_read_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-file-read-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-manager-registry-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_manager_registry_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-manager-registry-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-source-manager-query-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_source_manager_query_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-source-manager-query-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-domain-enums-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_domain_enums_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-domain-enums-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-layout-values-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_layout_values_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-layout-values-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-aggregate-request-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_aggregate_request_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-aggregate-request-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-windows-factories-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_windows_factories_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-windows-factories-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-builtin-layout-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_builtin_layout_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-builtin-layout-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-checked-arithmetic-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_checked_arithmetic_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-checked-arithmetic-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-power-of-two-alignment-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_power_of_two_alignment_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-power-of-two-alignment-gate"
        --build-mode "$<CONFIG>")

if(NSC_COMPILER_SUITE_NSC_EXECUTABLE AND
   NSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE AND
   NSC_COMPILER_SUITE_BUILD_ID_HEADER)
    add_test(
        NAME nsc.compiler-suite.host-basic-build-identity
        COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_build_identity_gate.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
            --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-build-identity-gate"
            --build-mode "$<CONFIG>"
            --nsc-executable "${NSC_COMPILER_SUITE_NSC_EXECUTABLE}"
            --comparison-nsc-executable "${NSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE}"
            --build-id-header "${NSC_COMPILER_SUITE_BUILD_ID_HEADER}")
endif()
