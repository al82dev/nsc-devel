#include "nsc_suite/TestRegistry.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <crtdbg.h>
#include <cstdlib>
#include <windows.h>
#endif

namespace
{
#if defined(_WIN32)
void configureNonInteractiveFailureReporting() noexcept
{
    // Automated cases must terminate into their captured pipes. A modal CRT or
    // Windows fault dialog would stall the exact case indefinitely and prevent
    // the controller from recording the remaining independent results.
    SetErrorMode(
        GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}
#else
void configureNonInteractiveFailureReporting() noexcept
{
}
#endif

struct Options
{
    bool list = false;
    bool all = false;
    std::optional<std::string> caseId;
    std::optional<std::string> component;
};

Options parseOptions(int argumentCount, char** arguments)
{
    Options options;
    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string_view argument(arguments[index]);
        if (argument == "--list")
        {
            options.list = true;
        }
        else if (argument == "--all")
        {
            options.all = true;
        }
        else if (argument == "--case" && index + 1 < argumentCount)
        {
            options.caseId = arguments[++index];
        }
        else if (argument == "--component" && index + 1 < argumentCount)
        {
            options.component = arguments[++index];
        }
        else
        {
            throw std::invalid_argument("unknown or incomplete argument: " + std::string(argument));
        }
    }
    const int selectors = static_cast<int>(options.list) + static_cast<int>(options.all) +
                          static_cast<int>(options.caseId.has_value()) +
                          static_cast<int>(options.component.has_value());
    if (selectors != 1)
    {
        throw std::invalid_argument(
            "select exactly one of --list, --all, --case, or --component");
    }
    return options;
}

std::string escapeJson(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}
}

int main(int argumentCount, char** arguments)
{
    configureNonInteractiveFailureReporting();
    try
    {
        const Options options = parseOptions(argumentCount, arguments);
        nsc::suite::TestRegistry registry;
#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
        // The destructive runner must remain a five-case process boundary.  Reusing the ordinary
        // registration graph here would duplicate the complete host suite and would also expose
        // its test-only global allocator to unrelated cases.
        nsc::suite::registerAstDestructiveModule(registry);
#elif defined(NSC_SOURCE_EXTENDED_TESTS_ENABLED)
        // The extended Source runner is a three-case process boundary.  It keeps the 4 GiB and
        // coordinated filesystem mutations out of the ordinary --all registry.
        nsc::suite::registerSourceFileOwnershipModule(registry);
        nsc::suite::registerSourceFileReadModule(registry);
#else
        nsc::suite::registerInfrastructureModule(registry);
        nsc::suite::registerBasicValuesModule(registry);
        nsc::suite::registerBasicSha256Module(registry);
        nsc::suite::registerBasicSha256FileModule(registry);
        nsc::suite::registerBasicOutputFailureModule(registry);
        nsc::suite::registerBasicArtifactStagingModule(registry);
        nsc::suite::registerBasicArtifactPreflightModule(registry);
        nsc::suite::registerBasicArtifactControlledPreflightModule(registry);
        nsc::suite::registerBasicArtifactCommitModule(registry);
        nsc::suite::registerBasicArtifactRollbackModule(registry);
        nsc::suite::registerBasicTransactionalWriterModule(registry);
        nsc::suite::registerBasicArtifactSeedModule(registry);
        nsc::suite::registerSourcePositionModule(registry);
        nsc::suite::registerSourceFileOwnershipModule(registry);
        nsc::suite::registerSourceLineIndexModule(registry);
        nsc::suite::registerSourceOffsetPositionModule(registry);
        nsc::suite::registerSourceLineTextModule(registry);
        nsc::suite::registerSourceFileReadModule(registry);
        nsc::suite::registerSourceManagerRegistryModule(registry);
        nsc::suite::registerSourceManagerQueryModule(registry);
        nsc::suite::registerTargetDomainEnumsModule(registry);
        nsc::suite::registerTargetLayoutValuesModule(registry);
        nsc::suite::registerTargetAggregateRequestModule(registry);
        nsc::suite::registerTargetWindowsFactoriesModule(registry);
        nsc::suite::registerTargetBuiltinLayoutModule(registry);
        nsc::suite::registerTargetCheckedArithmeticModule(registry);
        nsc::suite::registerTargetPowerOfTwoAlignmentModule(registry);
        nsc::suite::registerTargetElementStrideModule(registry);
        nsc::suite::registerTargetArrayLayoutModule(registry);
        nsc::suite::registerTargetPlainStructLayoutModule(registry);
        nsc::suite::registerTargetAggregateOptionsModule(registry);
        nsc::suite::registerTargetPackedFieldPlacementModule(registry);
        nsc::suite::registerTargetMinimumAlignmentModule(registry);
        nsc::suite::registerTargetExplicitOffsetModule(registry);
        nsc::suite::registerTargetUnionLayoutModule(registry);
        nsc::suite::registerTargetFlexibleArrayLayoutModule(registry);
        nsc::suite::registerTargetBitFieldValidationModule(registry);
        nsc::suite::registerTargetSequentialBitFieldModule(registry);
        nsc::suite::registerTargetZeroWidthBitFieldModule(registry);
        nsc::suite::registerTargetUnionBitFieldModule(registry);
        nsc::suite::registerTargetAggregateFinalizationModule(registry);
        nsc::suite::registerTargetIdentityFactoriesModule(registry);
        nsc::suite::registerTargetCpuFeatureSelectionModule(registry);
        nsc::suite::registerTargetDisplayNamesModule(registry);
        nsc::suite::registerTargetLlvmIdentityModule(registry);
        nsc::suite::registerTargetMachineStringsModule(registry);
        nsc::suite::registerTargetCpuFeatureSupportModule(registry);
        nsc::suite::registerTargetX64AssemblyRegisterModule(registry);
        nsc::suite::registerTargetArm64AssemblyRegisterModule(registry);
        nsc::suite::registerTargetAssemblyFeatureRequirementModule(registry);
        nsc::suite::registerDiagnosticSeverityModule(registry);
        nsc::suite::registerDiagnosticIdentityModule(registry);
        nsc::suite::registerDiagnosticRecordModule(registry);
        nsc::suite::registerDiagnosticEngineReportModule(registry);
        nsc::suite::registerDiagnosticEngineBudgetModule(registry);
        nsc::suite::registerDiagnosticEngineAppendModule(registry);
        nsc::suite::registerDiagnosticEngineMergeModule(registry);
        nsc::suite::registerDiagnosticEngineStateModule(registry);
        nsc::suite::registerDiagnosticRendererHeaderModule(registry);
        nsc::suite::registerDiagnosticRendererMarkerModule(registry);
        nsc::suite::registerDiagnosticRendererSecondaryModule(registry);
        nsc::suite::registerDiagnosticRendererEngineModule(registry);
        nsc::suite::registerLexerTokenKindModule(registry);
        nsc::suite::registerLexerTokenValueModule(registry);
        nsc::suite::registerLexerTokenDumperModule(registry);
        nsc::suite::registerLexerCursorTriviaModule(registry);
        nsc::suite::registerLexerIdentifierModule(registry);
        nsc::suite::registerLexerIntegerModule(registry);
        nsc::suite::registerLexerFloatingModule(registry);
        nsc::suite::registerLexerCharacterLiteralModule(registry);
        nsc::suite::registerLexerStringLiteralModule(registry);
        nsc::suite::registerLexerRawStringModule(registry);
        nsc::suite::registerLexerInterpolatedStringModule(registry);
        nsc::suite::registerLexerOperatorModule(registry);
        nsc::suite::registerLexerDiagnosticRecoveryModule(registry);
        nsc::suite::registerLexerAssemblyModule(registry);
        nsc::suite::registerAstArenaModule(registry);
        nsc::suite::registerAstNodeKindModule(registry);
        nsc::suite::registerAstRecordsModule(registry);
        nsc::suite::registerAstNodeIdentityModule(registry);
        nsc::suite::registerAstNodeTypeAttachmentModule(registry);
        nsc::suite::registerAstNodeGenericModule(registry);
        nsc::suite::registerAstNodeLifetimeModule(registry);
        nsc::suite::registerAstNodeTopologyModule(registry);
        nsc::suite::registerAstNodeScalarStateModule(registry);
        nsc::suite::registerTypeSyntaxIdentityModule(registry);
        nsc::suite::registerTypeSyntaxOwnershipModule(registry);
        nsc::suite::registerTypeSyntaxArgumentsModule(registry);
        nsc::suite::registerTypeSyntaxFunctionModule(registry);
        nsc::suite::registerTypeSyntaxConstPackModule(registry);
        nsc::suite::registerTypeSyntaxModifierModule(registry);
        nsc::suite::registerTypeSyntaxOriginModule(registry);
        nsc::suite::registerAstDumperNodeModule(registry);
        nsc::suite::registerAstDumperTypeModule(registry);
        nsc::suite::registerAstDumperTraversalModule(registry);
        nsc::suite::registerParserCursorRecoveryModule(registry);
        nsc::suite::registerParserLookaheadModule(registry);
        nsc::suite::registerParserTypeFoundationModule(registry);
        nsc::suite::registerParserFunctionTypeContinuationModule(registry);
        nsc::suite::registerParserTypeModifierContinuationModule(registry);
        nsc::suite::registerParserGenericParameterContinuationModule(registry);
        nsc::suite::registerParserGenericConstraintContinuationModule(registry);
        nsc::suite::registerParserTypeArgumentContinuationModule(registry);
        nsc::suite::registerParserGenericLookaheadContinuationModule(registry);
        nsc::suite::registerParserImportNamespaceModule(registry);
        nsc::suite::registerParserGlobalDeclarationModule(registry);
        nsc::suite::registerParserDelegateModule(registry);
        nsc::suite::registerParserStructDeclarationModule(registry);
        nsc::suite::registerParserVariantDeclarationModule(registry);
        nsc::suite::registerParserOpaqueDeclarationModule(registry);
        nsc::suite::registerParserClassInterfaceModule(registry);
        nsc::suite::registerParserNestedDeclarationModule(registry);
        nsc::suite::registerParserNestedMemberDispatchModule(registry);
        nsc::suite::registerParserEventModule(registry);
        nsc::suite::registerParserPropertyIndexerModule(registry);
        nsc::suite::registerParserOperatorModule(registry);
        nsc::suite::registerParserOperatorConversionModule(registry);
        nsc::suite::registerParserConstructorDestructorModule(registry);
        nsc::suite::registerParserCallableDeclarationModule(registry);
        nsc::suite::registerParserCallableClauseModule(registry);
        nsc::suite::registerParserBlockStatementModule(registry);
        nsc::suite::registerParserStatementContinuationModule(registry);
        nsc::suite::registerParserInlineAssemblyContinuationModule(registry);
        nsc::suite::registerParserControlFlowContinuationModule(registry);
        nsc::suite::registerParserPatternContinuationModule(registry);
        nsc::suite::registerParserPatternSemanticsModule(registry);
        nsc::suite::registerParserMatchSemanticsModule(registry);
        nsc::suite::registerParserLoopComprehensionModule(registry);
        nsc::suite::registerParserForeachSemanticsModule(registry);
        nsc::suite::registerParserForeachContinuationModule(registry);
        nsc::suite::registerParserCleanupContinuationModule(registry);
        nsc::suite::registerParserExpressionPrecedenceContinuationModule(registry);
        nsc::suite::registerParserLambdaContinuationModule(registry);
        nsc::suite::registerParserLambdaLookaheadModule(registry);
        nsc::suite::registerParserUnaryExpressionModule(registry);
        nsc::suite::registerParserPostfixExpressionModule(registry);
        nsc::suite::registerParserPrimaryExpressionModule(registry);
        nsc::suite::registerParserInterpolationFinalModule(registry);
        nsc::suite::registerParserConstructionCloneFinalModule(registry);
        nsc::suite::registerParserStateRobustnessFinalModule(registry);
        nsc::suite::registerParserTypeIterationCounterfactualModule(registry);
        nsc::suite::registerParserPatternIterationCounterfactualModule(registry);
        nsc::suite::registerParserPatternTraversalCounterfactualModule(registry);
        nsc::suite::registerParserPatternConditionCounterfactualModule(registry);
        nsc::suite::registerParserPatternLoweringCounterfactualModule(registry);
        nsc::suite::registerParserPatternBindingEmissionCounterfactualModule(registry);
        nsc::suite::registerParserMatchExpressionAnalysisCounterfactualModule(registry);
        nsc::suite::registerParserConditionalPatternAnalysisCounterfactualModule(registry);
        nsc::suite::registerSemaConstantConversionModule(registry);
        nsc::suite::registerSemaEffectsExpressionModule(registry);
        nsc::suite::registerSemaScopeSymbolModule(registry);
        nsc::suite::registerSemaTypeSystemModule(registry);
        nsc::suite::registerSemaSemanticModelModule(registry);
        nsc::suite::registerSemaAnalyzerLifecycleModule(registry);
        nsc::suite::registerSemaStructLayoutModule(registry);
        nsc::suite::registerSemaEnumModule(registry);
        nsc::suite::registerSemaFunctionModule(registry);
        nsc::suite::registerSemaVariableModule(registry);
        nsc::suite::registerSemaStatementModule(registry);
        nsc::suite::registerSemaIdentifierLiteralModule(registry);
        nsc::suite::registerSemaOperatorModule(registry);
        nsc::suite::registerSemaAssignmentCallModule(registry);
        nsc::suite::registerSemaIndexMemberModule(registry);
        nsc::suite::registerSemaConversionLayout223To244Module(registry);
        nsc::suite::registerSemaAllocationType245To266Module(registry);
        nsc::suite::registerSemaNumericConversion267To280Module(registry);
        nsc::suite::registerSemaConstantEvaluation281To298Module(registry);
        nsc::suite::registerSemaFallthroughDump299To321Module(registry);
        nsc::suite::registerHIRCoreModule(registry);
        nsc::suite::registerHIRLoweringModule(registry);
        nsc::suite::registerHIRDumpModule(registry);
        nsc::suite::registerHIRVerifierModule(registry);
        nsc::suite::registerCfgModelTraversalModule(registry);
        nsc::suite::registerCfgBuilderModule(registry);
        nsc::suite::registerCfgDumperModule(registry);
        nsc::suite::registerCfgVerifierModule(registry);
        nsc::suite::registerDominanceModel001To064Module(registry);
        nsc::suite::registerDominanceBuilder065To128Module(registry);
        nsc::suite::registerDominanceTraversalDump129To192Module(registry);
        nsc::suite::registerDominanceVerifier193To256Module(registry);
        nsc::suite::registerMirModelModule(registry);
        nsc::suite::registerMirBuilderModule(registry);
        nsc::suite::registerMirDumpModule(registry);
        nsc::suite::registerMirVerifierModule(registry);
        nsc::suite::registerSsaModelModule(registry);
        nsc::suite::registerSsaBuilderModule(registry);
        nsc::suite::registerSsaDumpModule(registry);
        nsc::suite::registerSsaVerifierModule(registry);
        nsc::suite::registerPassProtocolModule(registry);
        nsc::suite::registerPassModelAnalysisModule(registry);
        nsc::suite::registerPassConstantEffectsModule(registry);
        nsc::suite::registerPassTransformsModule(registry);
        nsc::suite::registerPassManagerModule(registry);
        nsc::suite::registerPassDumpModule(registry);
        nsc::suite::registerPassVerifierModule(registry);
        nsc::suite::registerLanguageAbiSemanticsModule(registry);
        nsc::suite::registerLanguageAggregateSemanticsModule(registry);
        nsc::suite::registerLanguageCallableSemanticsModule(registry);
        nsc::suite::registerLanguageCompatibilitySemanticsModule(registry);
        nsc::suite::registerLanguageCoroutinePlanningModule(registry);
        nsc::suite::registerLanguageConcurrencyModule(registry);
        nsc::suite::registerLanguageLifetimeModule(registry);
        nsc::suite::registerLanguageErrorSemanticsModule(registry);
        nsc::suite::registerLanguageExpressionSemanticsModule(registry);
        nsc::suite::registerLanguageGenericSemanticsModule(registry);
        nsc::suite::registerLanguageIntegrationQualityModule(registry);
        nsc::suite::registerLanguageIterationSemanticsModule(registry);
        nsc::suite::registerLanguageSurfaceModule(registry);
        nsc::suite::registerLanguageMemoryModelModule(registry);
        nsc::suite::registerLanguageMetaprogrammingModule(registry);
        nsc::suite::registerLanguageModuleSemanticsModule(registry);
        nsc::suite::registerLanguageNumericCoreModule(registry);
        nsc::suite::registerLanguageCoreLibraryE2eModule(registry);
        nsc::suite::registerLanguageNumericAdvancedModule(registry);
        nsc::suite::registerLanguageOwnershipStateModule(registry);
        nsc::suite::registerLanguageOwnershipLifetimeModule(registry);
        nsc::suite::registerLanguageReleaseFreezeModule(registry);
        nsc::suite::registerLanguageReleaseConformanceModule(registry);
        nsc::suite::registerLanguageReleasePromotionModule(registry);
        nsc::suite::registerLanguageSmartPointerOperationsModule(registry);
        nsc::suite::registerLanguageSmartPointerContractModule(registry);
        nsc::suite::registerLanguageSumTypeLayoutModule(registry);
        nsc::suite::registerLanguagePatternSemanticsModule(registry);
        nsc::suite::registerLanguageTraitValidationModule(registry);
        nsc::suite::registerLanguageTraitDispatchModule(registry);
        nsc::suite::registerLanguageTraitOperatorModule(registry);
        nsc::suite::registerFrontendCoreModule(registry);
        nsc::suite::registerFrontendNamesMembersModule(registry);
        nsc::suite::registerFrontendGenericsFlowModule(registry);
        nsc::suite::registerFrontendMethodBodiesModule(registry);
        nsc::suite::registerFrontendAuxiliaryModelsModule(registry);
        nsc::suite::registerFrontendLoweringModule(registry);
        nsc::suite::registerLlvmFoundationModule(registry);
        nsc::suite::registerLlvmPolicyModule(registry);
        nsc::suite::registerLlvmObservationLowererModule(registry);
        nsc::suite::registerInteropMetadataModelModule(registry);
        nsc::suite::registerInteropCodecModule(registry);
        nsc::suite::registerInteropSurfacesModule(registry);
        nsc::suite::registerInteropValidationMergeModule(registry);
        nsc::suite::registerInteropDiffSchemaModule(registry);
        nsc::suite::registerLinkerBuildersModule(registry);
        nsc::suite::registerCompilationCore001To112Module(registry);
        nsc::suite::registerCompilationGraph113To168Module(registry);
        nsc::suite::registerCompilationXml169To216Module(registry);
        nsc::suite::registerCompilationProject217To336Module(registry);
        nsc::suite::registerCompilationSdk337To384Module(registry);
        nsc::suite::registerCompilationIdentity385To416Module(registry);
        nsc::suite::registerDriverFoundation001To096Module(registry);
        nsc::suite::registerDriverRuntime097To192Module(registry);
        nsc::suite::registerDriverMigration345To432Module(registry);
        nsc::suite::registerDriverProject497To616Module(registry);
        nsc::suite::registerLanguageServiceProtocol001To080Module(registry);
        nsc::suite::registerLanguageServiceDocuments081To160Module(registry);
        nsc::suite::registerLanguageServiceQueries233To312Module(registry);
        nsc::suite::registerLanguageServiceAnalysis313To336Module(registry);
        nsc::suite::registerRuntimeAbiIdentityModule(registry);
        nsc::suite::registerRuntimeAbiVerificationModule(registry);
        nsc::suite::registerRuntimeCapabilityModule(registry);
#endif
        if (options.list)
        {
            for (const auto& testCase : registry.cases())
            {
                std::cout << testCase.id << '\t' << testCase.contractId << '\t'
                          << testCase.component << '\n';
            }
            return 0;
        }
        std::vector<const nsc::suite::TestCase*> selected;
        if (options.caseId)
        {
            const auto* testCase = registry.find(*options.caseId);
            if (testCase == nullptr)
            {
                throw std::invalid_argument("unknown test case: " + *options.caseId);
            }
            selected.push_back(testCase);
        }
        else if (options.component)
        {
            selected = registry.byComponent(*options.component);
            if (selected.empty())
            {
                throw std::invalid_argument("component has no test cases: " + *options.component);
            }
        }
        else
        {
            selected = registry.all();
        }

        const auto results = registry.run(selected);
        std::size_t passed = 0;
        for (const auto& result : results)
        {
            passed += static_cast<std::size_t>(result.passed);
            std::cout << "{\"case_id\":\"" << escapeJson(result.id)
                      << "\",\"component\":\"" << escapeJson(result.component)
                      << "\",\"contract_id\":\"" << escapeJson(result.contractId)
                      << "\",\"duration_us\":" << result.duration.count()
                      << ",\"status\":\"" << (result.passed ? "PASS" : "FAIL")
                      << "\",\"failures\":[";
            for (std::size_t index = 0; index < result.failures.size(); ++index)
            {
                std::cout << (index == 0 ? "" : ",") << '"'
                          << escapeJson(result.failures[index]) << '"';
            }
            std::cout << "]}\n";
        }
        return passed == results.size() ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "nsc_component_tests: error: " << error.what() << '\n';
        return 2;
    }
}
