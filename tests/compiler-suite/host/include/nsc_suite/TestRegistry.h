#pragma once

#include "nsc_suite/TestContext.h"

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace nsc::suite
{
using TestFunction = std::function<void(TestContext&)>;

enum class TestDisposition
{
    Required,
    KnownProductDefect,
};

struct TestCase
{
    std::string id;
    std::string contractId;
    std::string component;
    std::string layer;
    std::string profile;
    std::vector<std::string> tags;
    TestFunction function;
    TestDisposition disposition = TestDisposition::Required;
};

struct TestResult
{
    std::string id;
    std::string contractId;
    std::string component;
    bool passed = false;
    std::chrono::microseconds duration{};
    std::vector<std::string> failures;
};

class TestRegistry
{
public:
    void add(TestCase testCase);

    [[nodiscard]] const std::vector<TestCase>& cases() const;
    [[nodiscard]] const TestCase* find(std::string_view id) const;
    [[nodiscard]] std::vector<const TestCase*> all() const;
    [[nodiscard]] std::vector<const TestCase*> byComponent(std::string_view component) const;
    [[nodiscard]] std::vector<TestResult> run(const std::vector<const TestCase*>& selected) const;

private:
    void ensureOrdered() const;

    mutable std::vector<TestCase> cases_;
    std::unordered_set<std::string> identifiers_;
    mutable bool ordered_ = true;
};

void registerInfrastructureModule(TestRegistry& registry);
void registerBasicValuesModule(TestRegistry& registry);
void registerBasicSha256Module(TestRegistry& registry);
void registerBasicSha256FileModule(TestRegistry& registry);
void registerBasicOutputFailureModule(TestRegistry& registry);
void registerBasicArtifactStagingModule(TestRegistry& registry);
void registerBasicArtifactPreflightModule(TestRegistry& registry);
void registerBasicArtifactControlledPreflightModule(TestRegistry& registry);
void registerBasicArtifactCommitModule(TestRegistry& registry);
void registerBasicArtifactRollbackModule(TestRegistry& registry);
void registerBasicTransactionalWriterModule(TestRegistry& registry);
void registerBasicArtifactSeedModule(TestRegistry& registry);
void registerSourcePositionModule(TestRegistry& registry);
void registerSourceFileOwnershipModule(TestRegistry& registry);
void registerSourceLineIndexModule(TestRegistry& registry);
void registerSourceOffsetPositionModule(TestRegistry& registry);
void registerSourceLineTextModule(TestRegistry& registry);
void registerSourceFileReadModule(TestRegistry& registry);
void registerSourceManagerRegistryModule(TestRegistry& registry);
void registerSourceManagerQueryModule(TestRegistry& registry);
void registerTargetDomainEnumsModule(TestRegistry& registry);
void registerTargetLayoutValuesModule(TestRegistry& registry);
void registerTargetAggregateRequestModule(TestRegistry& registry);
void registerTargetWindowsFactoriesModule(TestRegistry& registry);
void registerTargetBuiltinLayoutModule(TestRegistry& registry);
void registerTargetCheckedArithmeticModule(TestRegistry& registry);
void registerTargetPowerOfTwoAlignmentModule(TestRegistry& registry);
void registerTargetElementStrideModule(TestRegistry& registry);
void registerTargetArrayLayoutModule(TestRegistry& registry);
void registerTargetPlainStructLayoutModule(TestRegistry& registry);
void registerTargetAggregateOptionsModule(TestRegistry& registry);
void registerTargetPackedFieldPlacementModule(TestRegistry& registry);
void registerTargetMinimumAlignmentModule(TestRegistry& registry);
void registerTargetExplicitOffsetModule(TestRegistry& registry);
void registerTargetUnionLayoutModule(TestRegistry& registry);
void registerTargetFlexibleArrayLayoutModule(TestRegistry& registry);
void registerTargetBitFieldValidationModule(TestRegistry& registry);
void registerTargetSequentialBitFieldModule(TestRegistry& registry);
void registerTargetZeroWidthBitFieldModule(TestRegistry& registry);
void registerTargetUnionBitFieldModule(TestRegistry& registry);
void registerTargetAggregateFinalizationModule(TestRegistry& registry);
void registerTargetIdentityFactoriesModule(TestRegistry& registry);
void registerTargetCpuFeatureSelectionModule(TestRegistry& registry);
void registerTargetDisplayNamesModule(TestRegistry& registry);
void registerTargetLlvmIdentityModule(TestRegistry& registry);
void registerTargetMachineStringsModule(TestRegistry& registry);
void registerTargetCpuFeatureSupportModule(TestRegistry& registry);
void registerTargetX64AssemblyRegisterModule(TestRegistry& registry);
void registerTargetArm64AssemblyRegisterModule(TestRegistry& registry);
void registerTargetAssemblyFeatureRequirementModule(TestRegistry& registry);
void registerDiagnosticSeverityModule(TestRegistry& registry);
void registerDiagnosticIdentityModule(TestRegistry& registry);
void registerDiagnosticRecordModule(TestRegistry& registry);
void registerDiagnosticEngineReportModule(TestRegistry& registry);
void registerDiagnosticEngineBudgetModule(TestRegistry& registry);
void registerDiagnosticEngineAppendModule(TestRegistry& registry);
void registerDiagnosticEngineMergeModule(TestRegistry& registry);
void registerDiagnosticEngineStateModule(TestRegistry& registry);
void registerDiagnosticRendererHeaderModule(TestRegistry& registry);
void registerDiagnosticRendererMarkerModule(TestRegistry& registry);
void registerDiagnosticRendererSecondaryModule(TestRegistry& registry);
void registerDiagnosticRendererEngineModule(TestRegistry& registry);
void registerLexerTokenKindModule(TestRegistry& registry);
void registerLexerTokenValueModule(TestRegistry& registry);
void registerLexerTokenDumperModule(TestRegistry& registry);
void registerLexerCursorTriviaModule(TestRegistry& registry);
void registerLexerIdentifierModule(TestRegistry& registry);
void registerLexerIntegerModule(TestRegistry& registry);
void registerLexerFloatingModule(TestRegistry& registry);
void registerLexerCharacterLiteralModule(TestRegistry& registry);
void registerLexerStringLiteralModule(TestRegistry& registry);
void registerLexerRawStringModule(TestRegistry& registry);
void registerLexerInterpolatedStringModule(TestRegistry& registry);
void registerLexerOperatorModule(TestRegistry& registry);
void registerLexerDiagnosticRecoveryModule(TestRegistry& registry);
void registerLexerAssemblyModule(TestRegistry& registry);
void registerAstArenaModule(TestRegistry& registry);
void registerAstNodeKindModule(TestRegistry& registry);
void registerAstRecordsModule(TestRegistry& registry);
void registerAstNodeIdentityModule(TestRegistry& registry);
void registerAstNodeTypeAttachmentModule(TestRegistry& registry);
void registerAstNodeGenericModule(TestRegistry& registry);
void registerAstNodeLifetimeModule(TestRegistry& registry);
void registerAstNodeTopologyModule(TestRegistry& registry);
void registerAstNodeScalarStateModule(TestRegistry& registry);
void registerAstDestructiveModule(TestRegistry& registry);
void registerTypeSyntaxIdentityModule(TestRegistry& registry);
void registerTypeSyntaxOwnershipModule(TestRegistry& registry);
void registerTypeSyntaxArgumentsModule(TestRegistry& registry);
void registerTypeSyntaxFunctionModule(TestRegistry& registry);
void registerTypeSyntaxConstPackModule(TestRegistry& registry);
void registerTypeSyntaxModifierModule(TestRegistry& registry);
void registerTypeSyntaxOriginModule(TestRegistry& registry);
void registerAstDumperNodeModule(TestRegistry& registry);
void registerAstDumperTypeModule(TestRegistry& registry);
void registerAstDumperTraversalModule(TestRegistry& registry);
void registerParserCursorRecoveryModule(TestRegistry& registry);
void registerParserLookaheadModule(TestRegistry& registry);
void registerParserTypeFoundationModule(TestRegistry& registry);
void registerParserFunctionTypeContinuationModule(TestRegistry& registry);
void registerParserTypeModifierContinuationModule(TestRegistry& registry);
void registerParserGenericParameterContinuationModule(TestRegistry& registry);
void registerParserGenericConstraintContinuationModule(TestRegistry& registry);
void registerParserTypeArgumentContinuationModule(TestRegistry& registry);
void registerParserGenericLookaheadContinuationModule(TestRegistry& registry);
void registerParserImportNamespaceModule(TestRegistry& registry);
void registerParserGlobalDeclarationModule(TestRegistry& registry);
void registerParserDelegateModule(TestRegistry& registry);
void registerParserStructDeclarationModule(TestRegistry& registry);
void registerParserVariantDeclarationModule(TestRegistry& registry);
void registerParserOpaqueDeclarationModule(TestRegistry& registry);
void registerParserClassInterfaceModule(TestRegistry& registry);
void registerParserNestedDeclarationModule(TestRegistry& registry);
void registerParserNestedMemberDispatchModule(TestRegistry& registry);
void registerParserEventModule(TestRegistry& registry);
void registerParserPropertyIndexerModule(TestRegistry& registry);
void registerParserOperatorModule(TestRegistry& registry);
void registerParserOperatorConversionModule(TestRegistry& registry);
void registerParserConstructorDestructorModule(TestRegistry& registry);
void registerParserCallableDeclarationModule(TestRegistry& registry);
void registerParserCallableClauseModule(TestRegistry& registry);
void registerParserBlockStatementModule(TestRegistry& registry);
void registerParserStatementContinuationModule(TestRegistry& registry);
void registerParserInlineAssemblyContinuationModule(TestRegistry& registry);
void registerParserControlFlowContinuationModule(TestRegistry& registry);
void registerParserPatternContinuationModule(TestRegistry& registry);
void registerParserPatternSemanticsModule(TestRegistry& registry);
void registerParserMatchSemanticsModule(TestRegistry& registry);
void registerParserLoopComprehensionModule(TestRegistry& registry);
void registerParserForeachSemanticsModule(TestRegistry& registry);
void registerParserForeachContinuationModule(TestRegistry& registry);
void registerParserCleanupContinuationModule(TestRegistry& registry);
void registerParserExpressionPrecedenceContinuationModule(TestRegistry& registry);
void registerParserLambdaContinuationModule(TestRegistry& registry);
void registerParserLambdaLookaheadModule(TestRegistry& registry);
void registerParserUnaryExpressionModule(TestRegistry& registry);
void registerParserPostfixExpressionModule(TestRegistry& registry);
void registerParserPrimaryExpressionModule(TestRegistry& registry);
void registerParserInterpolationFinalModule(TestRegistry& registry);
void registerParserConstructionCloneFinalModule(TestRegistry& registry);
void registerParserStateRobustnessFinalModule(TestRegistry& registry);
void registerParserTypeIterationCounterfactualModule(TestRegistry& registry);
void registerParserPatternIterationCounterfactualModule(TestRegistry& registry);
void registerParserPatternTraversalCounterfactualModule(TestRegistry& registry);
void registerParserPatternConditionCounterfactualModule(TestRegistry& registry);
void registerParserPatternLoweringCounterfactualModule(TestRegistry& registry);
void registerParserPatternBindingEmissionCounterfactualModule(TestRegistry& registry);
void registerParserMatchExpressionAnalysisCounterfactualModule(TestRegistry& registry);
void registerParserConditionalPatternAnalysisCounterfactualModule(TestRegistry& registry);
void registerParserMatchStatementAnalysisCounterfactualModule(TestRegistry& registry);
void registerParserMatchIntervalIndexCounterfactualModule(TestRegistry& registry);
void registerParserAssignmentIterationCounterfactualModule(TestRegistry& registry);
void registerParserConditionalQuestionIndexCounterfactualModule(TestRegistry& registry);
void registerParserNullCoalescingIterationCounterfactualModule(TestRegistry& registry);
void registerParserConditionalTrampolineCounterfactualModule(TestRegistry& registry);
void registerParserLambdaBlockContinuationCounterfactualModule(TestRegistry& registry);
void registerParserStructuredControlContinuationCounterfactualModule(TestRegistry& registry);
void registerParserForeachCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserTryContinuationCounterfactualModule(TestRegistry& registry);
void registerParserUsingCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserDeferCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserSwitchCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserMatchCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserConditionalPatternCoordinatorCounterfactualModule(TestRegistry& registry);
void registerParserSpecialStatementSccClosureCounterfactualModule(TestRegistry& registry);
void registerSemaConstantConversionModule(TestRegistry& registry);
void registerSemaEffectsExpressionModule(TestRegistry& registry);
void registerSemaScopeSymbolModule(TestRegistry& registry);
void registerSemaTypeSystemModule(TestRegistry& registry);
void registerSemaSemanticModelModule(TestRegistry& registry);
void registerSemaAnalyzerLifecycleModule(TestRegistry& registry);
void registerSemaStructLayoutModule(TestRegistry& registry);
void registerSemaEnumModule(TestRegistry& registry);
void registerSemaFunctionModule(TestRegistry& registry);
void registerSemaVariableModule(TestRegistry& registry);
void registerSemaStatementModule(TestRegistry& registry);
void registerSemaIdentifierLiteralModule(TestRegistry& registry);
void registerSemaOperatorModule(TestRegistry& registry);
void registerSemaAssignmentCallModule(TestRegistry& registry);
void registerSemaIndexMemberModule(TestRegistry& registry);
void registerSemaConversionLayout223To244Module(TestRegistry& registry);
void registerSemaAllocationType245To266Module(TestRegistry& registry);
void registerSemaNumericConversion267To280Module(TestRegistry& registry);
void registerSemaConstantEvaluation281To298Module(TestRegistry& registry);
void registerSemaFallthroughDump299To321Module(TestRegistry& registry);
void registerHIRCoreModule(TestRegistry& registry);
void registerHIRLoweringModule(TestRegistry& registry);
void registerHIRDumpModule(TestRegistry& registry);
void registerHIRVerifierModule(TestRegistry& registry);
void registerCfgModelTraversalModule(TestRegistry& registry);
void registerCfgBuilderModule(TestRegistry& registry);
void registerCfgDumperModule(TestRegistry& registry);
void registerCfgVerifierModule(TestRegistry& registry);
void registerDominanceModel001To064Module(TestRegistry& registry);
void registerDominanceBuilder065To128Module(TestRegistry& registry);
void registerDominanceTraversalDump129To192Module(TestRegistry& registry);
void registerDominanceVerifier193To256Module(TestRegistry& registry);
void registerMirModelModule(TestRegistry& registry);
void registerMirBuilderModule(TestRegistry& registry);
void registerMirDumpModule(TestRegistry& registry);
void registerMirVerifierModule(TestRegistry& registry);
void registerSsaModelModule(TestRegistry& registry);
void registerSsaBuilderModule(TestRegistry& registry);
void registerSsaDumpModule(TestRegistry& registry);
void registerSsaVerifierModule(TestRegistry& registry);
void registerPassProtocolModule(TestRegistry& registry);
void registerPassModelAnalysisModule(TestRegistry& registry);
void registerPassConstantEffectsModule(TestRegistry& registry);
void registerPassTransformsModule(TestRegistry& registry);
void registerPassManagerModule(TestRegistry& registry);
void registerPassDumpModule(TestRegistry& registry);
void registerPassVerifierModule(TestRegistry& registry);
void registerLanguageAbiSemanticsModule(TestRegistry& registry);
void registerLanguageAggregateSemanticsModule(TestRegistry& registry);
void registerLanguageCallableSemanticsModule(TestRegistry& registry);
void registerLanguageCompatibilitySemanticsModule(TestRegistry& registry);
void registerLanguageCoroutinePlanningModule(TestRegistry& registry);
void registerLanguageConcurrencyModule(TestRegistry& registry);
void registerLanguageLifetimeModule(TestRegistry& registry);
void registerLanguageErrorSemanticsModule(TestRegistry& registry);
void registerLanguageExpressionSemanticsModule(TestRegistry& registry);
void registerLanguageGenericSemanticsModule(TestRegistry& registry);
void registerLanguageIntegrationQualityModule(TestRegistry& registry);
void registerLanguageIterationSemanticsModule(TestRegistry& registry);
void registerLanguageSurfaceModule(TestRegistry& registry);
void registerLanguageMemoryModelModule(TestRegistry& registry);
void registerLanguageMetaprogrammingModule(TestRegistry& registry);
void registerLanguageModuleSemanticsModule(TestRegistry& registry);
void registerLanguageNumericCoreModule(TestRegistry& registry);
void registerLanguageCoreLibraryE2eModule(TestRegistry& registry);
void registerLanguageNumericAdvancedModule(TestRegistry& registry);
void registerLanguageOwnershipStateModule(TestRegistry& registry);
void registerLanguageOwnershipLifetimeModule(TestRegistry& registry);
void registerLanguageReleaseFreezeModule(TestRegistry& registry);
void registerLanguageReleaseConformanceModule(TestRegistry& registry);
void registerLanguageReleasePromotionModule(TestRegistry& registry);
void registerLanguageSmartPointerOperationsModule(TestRegistry& registry);
void registerLanguageSmartPointerContractModule(TestRegistry& registry);
void registerLanguageSumTypeLayoutModule(TestRegistry& registry);
void registerLanguagePatternSemanticsModule(TestRegistry& registry);
void registerLanguageTraitValidationModule(TestRegistry& registry);
void registerLanguageTraitDispatchModule(TestRegistry& registry);
void registerLanguageTraitOperatorModule(TestRegistry& registry);
void registerFrontendCoreModule(TestRegistry& registry);
void registerFrontendNamesMembersModule(TestRegistry& registry);
void registerFrontendGenericsFlowModule(TestRegistry& registry);
void registerFrontendMethodBodiesModule(TestRegistry& registry);
void registerFrontendAuxiliaryModelsModule(TestRegistry& registry);
void registerFrontendLoweringModule(TestRegistry& registry);
void registerLlvmFoundationModule(TestRegistry& registry);
void registerLlvmPolicyModule(TestRegistry& registry);
void registerLlvmObservationLowererModule(TestRegistry& registry);
void registerInteropMetadataModelModule(TestRegistry& registry);
void registerInteropCodecModule(TestRegistry& registry);
void registerInteropSurfacesModule(TestRegistry& registry);
void registerInteropValidationMergeModule(TestRegistry& registry);
void registerInteropDiffSchemaModule(TestRegistry& registry);
void registerLinkerBuildersModule(TestRegistry& registry);
void registerCompilationCore001To112Module(TestRegistry& registry);
void registerCompilationGraph113To168Module(TestRegistry& registry);
void registerCompilationXml169To216Module(TestRegistry& registry);
void registerCompilationProject217To336Module(TestRegistry& registry);
void registerCompilationSdk337To384Module(TestRegistry& registry);
void registerCompilationIdentity385To416Module(TestRegistry& registry);
void registerDriverFoundation001To096Module(TestRegistry& registry);
void registerDriverRuntime097To192Module(TestRegistry& registry);
void registerDriverMigration345To432Module(TestRegistry& registry);
void registerDriverProject497To616Module(TestRegistry& registry);
void registerLanguageServiceProtocol001To080Module(TestRegistry& registry);
void registerLanguageServiceDocuments081To160Module(TestRegistry& registry);
void registerLanguageServiceQueries233To312Module(TestRegistry& registry);
void registerLanguageServiceAnalysis313To336Module(TestRegistry& registry);
void registerRuntimeAbiIdentityModule(TestRegistry& registry);
void registerRuntimeAbiVerificationModule(TestRegistry& registry);
void registerRuntimeCapabilityModule(TestRegistry& registry);
}
