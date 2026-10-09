#include "nsc_suite/TestRegistry.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
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
        throw std::invalid_argument("select exactly one of --list, --all, --case, or --component");
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
    try
    {
        const Options options = parseOptions(argumentCount, arguments);
        nsc::suite::TestRegistry registry;
        nsc::suite::registerInfrastructureModule(registry);
        nsc::suite::registerBasicValuesModule(registry);
        nsc::suite::registerBasicSha256Module(registry);
        nsc::suite::registerBasicSha256FileModule(registry);
        nsc::suite::registerBasicOutputFailureModule(registry);
        nsc::suite::registerBasicArtifactStagingModule(registry);
        nsc::suite::registerBasicArtifactPreflightModule(registry);
        nsc::suite::registerBasicArtifactControlledPreflightModule(registry);
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
