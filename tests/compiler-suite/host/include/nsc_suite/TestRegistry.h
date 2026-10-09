#pragma once

#include "nsc_suite/TestContext.h"

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
using TestFunction = std::function<void(TestContext&)>;

struct TestCase
{
    std::string id;
    std::string contractId;
    std::string component;
    std::string layer;
    std::string profile;
    std::vector<std::string> tags;
    TestFunction function;
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

    [[nodiscard]] const std::vector<TestCase>& cases() const noexcept;
    [[nodiscard]] const TestCase* find(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<const TestCase*> all() const;
    [[nodiscard]] std::vector<const TestCase*> byComponent(std::string_view component) const;
    [[nodiscard]] std::vector<TestResult> run(const std::vector<const TestCase*>& selected) const;

private:
    std::vector<TestCase> cases_;
};

void registerInfrastructureModule(TestRegistry& registry);
void registerBasicValuesModule(TestRegistry& registry);
void registerBasicSha256Module(TestRegistry& registry);
void registerBasicSha256FileModule(TestRegistry& registry);
void registerBasicOutputFailureModule(TestRegistry& registry);
void registerBasicArtifactStagingModule(TestRegistry& registry);
void registerBasicArtifactPreflightModule(TestRegistry& registry);
void registerBasicArtifactControlledPreflightModule(TestRegistry& registry);
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
}
