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
    host/modules/TargetElementStrideModule.cpp
    host/modules/TargetArrayLayoutModule.cpp
    host/modules/TargetPlainStructLayoutModule.cpp
    host/modules/TargetAggregateOptionsModule.cpp
    host/modules/TargetPackedFieldPlacementModule.cpp
    host/modules/TargetMinimumAlignmentModule.cpp
    host/modules/TargetExplicitOffsetModule.cpp
    host/modules/TargetUnionLayoutModule.cpp
    host/modules/TargetFlexibleArrayLayoutModule.cpp
    host/modules/TargetBitFieldValidationModule.cpp
    host/modules/TargetSequentialBitFieldModule.cpp
    host/modules/TargetZeroWidthBitFieldModule.cpp
    host/modules/TargetUnionBitFieldModule.cpp
    host/modules/TargetAggregateFinalizationModule.cpp
    host/modules/TargetIdentityFactoriesModule.cpp
    host/modules/TargetCpuFeatureSelectionModule.cpp
    host/modules/TargetDisplayNamesModule.cpp
    host/modules/TargetLlvmIdentityModule.cpp
    host/modules/TargetMachineStringsModule.cpp
    host/modules/TargetCpuFeatureSupportModule.cpp
    host/modules/TargetX64AssemblyRegisterModule.cpp
    host/modules/TargetArm64AssemblyRegisterModule.cpp
    host/modules/TargetAssemblyFeatureRequirementModule.cpp
    host/modules/DiagnosticSeverityModule.cpp
    host/modules/DiagnosticIdentityModule.cpp
    host/modules/DiagnosticRecordModule.cpp
    host/modules/DiagnosticEngineReportModule.cpp
    host/modules/DiagnosticEngineBudgetModule.cpp
    host/modules/DiagnosticEngineAppendModule.cpp
    host/modules/DiagnosticEngineMergeModule.cpp
    host/modules/DiagnosticEngineStateModule.cpp
    host/modules/DiagnosticRendererHeaderModule.cpp
    host/modules/DiagnosticRendererMarkerModule.cpp
    host/modules/DiagnosticRendererSecondaryModule.cpp
    host/modules/DiagnosticRendererEngineModule.cpp
    host/modules/LanguageAbiSemanticsModule.cpp
    host/modules/LanguageAggregateSemanticsModule.cpp
    host/modules/LanguageCallableSemanticsModule.cpp
    host/modules/LanguageCompatibilitySemanticsModule.cpp
    host/modules/LanguageCoroutinePlanningModule.cpp
    host/modules/LanguageConcurrencyModule.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Basic/Sha256.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Basic/ArtifactTransaction.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Source/SourceFile.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Source/SourceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Target/TargetDataLayout.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Target/TargetInfo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Diagnostics/Diagnostic.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Diagnostics/DiagnosticEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Diagnostics/DiagnosticRenderer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/AbiSemantics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/AggregateSemantics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/CallableSemantics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/CompatibilitySemantics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/IterationSemantics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/Language/CoroutineSemantics.cpp")

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
add_test(
    NAME nsc.compiler-suite.host-target-element-stride-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_element_stride_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-element-stride-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-array-layout-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_array_layout_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-array-layout-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-plain-struct-layout-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_plain_struct_layout_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-plain-struct-layout-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-aggregate-options-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_aggregate_options_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-aggregate-options-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-packed-field-placement-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_packed_field_placement_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-packed-field-placement-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-minimum-alignment-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_minimum_alignment_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-minimum-alignment-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-explicit-offset-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_explicit_offset_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-explicit-offset-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-union-layout-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_union_layout_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-union-layout-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-flexible-array-layout-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_flexible_array_layout_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-flexible-array-layout-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-bit-field-validation-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_bit_field_validation_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-bit-field-validation-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-sequential-bit-field-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_sequential_bit_field_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-sequential-bit-field-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-zero-width-bit-field-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_zero_width_bit_field_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-zero-width-bit-field-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-union-bit-field-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_union_bit_field_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-union-bit-field-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-aggregate-finalization-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_aggregate_finalization_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-aggregate-finalization-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-identity-factories-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_identity_factories_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-identity-factories-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-cpu-feature-selection-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_cpu_feature_selection_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-cpu-feature-selection-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-display-names-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_display_names_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-display-names-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-llvm-identity-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_llvm_identity_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-llvm-identity-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-machine-strings-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_machine_strings_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-machine-strings-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-cpu-feature-support-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_cpu_feature_support_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-cpu-feature-support-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-x64-assembly-register-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_x64_assembly_register_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-x64-assembly-register-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-arm64-assembly-register-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_arm64_assembly_register_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-arm64-assembly-register-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-target-assembly-feature-requirement-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_target_assembly_feature_requirement_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-target-assembly-feature-requirement-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-severity-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_severity_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-severity-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-identity-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_identity_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-identity-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-record-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_record_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-record-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-engine-report-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_engine_report_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-engine-report-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-engine-budget-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_engine_budget_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-engine-budget-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-engine-append-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_engine_append_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-engine-append-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-engine-merge-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_engine_merge_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-engine-merge-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-engine-state-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_engine_state_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-engine-state-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-renderer-header-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_renderer_header_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-renderer-header-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-renderer-marker-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_renderer_marker_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-renderer-marker-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-renderer-secondary-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_renderer_secondary_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-renderer-secondary-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-diagnostics-renderer-engine-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_diagnostics_renderer_engine_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-diagnostics-renderer-engine-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-language-abi-semantics-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_language_abi_semantics_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-language-abi-semantics-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-language-aggregate-semantics-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_language_aggregate_semantics_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-language-aggregate-semantics-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-language-callable-semantics-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_language_callable_semantics_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-language-callable-semantics-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-language-compatibility-semantics-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_language_compatibility_semantics_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-language-compatibility-semantics-gate"
        --build-mode "$<CONFIG>")
add_test(
    NAME nsc.compiler-suite.host-language-coroutine-planning-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_language_coroutine_planning_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-language-coroutine-planning-gate"
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
