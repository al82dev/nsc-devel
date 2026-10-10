#include "SelfHostSemaTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_sema_support::PendingContractRange;

constexpr std::string_view kAnalysisRequirement =
    "the controller has no executable NativeSharp selfhost semantic-analysis boundary with full syntax trees, "
    "ownership storage graphs, analysis modes, dumps, and exact diagnostic provenance";

constexpr std::array<PendingContractRange, 5> kContracts{{
    {813, 819, "CSH-SEM-016", "component", "semantic fact symbol and diagnostic per expression",
     kAnalysisRequirement},
    {820, 826, "CSH-SEM-017", "component", "scopes facts and diagnostics in source order",
     kAnalysisRequirement},
    {827, 833, "CSH-SEM-018", "component", "storage origin traversal and diagnostic location",
     kAnalysisRequirement},
    {834, 840, "CSH-SEM-019", "integration", "symbol/scope/fact counts exports and errors",
     kAnalysisRequirement},
    {841, 848, "CSH-SEM-020", "unit", "exact normalized structural dump", kAnalysisRequirement},
}};
}

void registerSelfHostSemaAnalysisModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_sema_support::addInfrastructurePendingRange(registry, range);
    }
}
}
