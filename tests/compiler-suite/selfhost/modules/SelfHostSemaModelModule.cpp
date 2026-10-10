#include "SelfHostSemaTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_sema_support::PendingContractRange;

constexpr std::string_view kModelRequirement =
    "the controller has no executable NativeSharp selfhost semantic-model boundary with owned syntax, "
    "type, symbol, scope, graph, and diagnostic observation";

constexpr std::array<PendingContractRange, 5> kContracts{{
    {696, 701, "CSH-SEM-001", "unit", "fields conversions bindings counts and error transitions",
     kModelRequirement},
    {702, 710, "CSH-SEM-002", "component", "nodes edges order and text", kModelRequirement},
    {711, 719, "CSH-SEM-003", "component", "region hierarchy and dump", kModelRequirement},
    {720, 728, "CSH-SEM-004", "component", "count identities ordering and dump", kModelRequirement},
    {729, 736, "CSH-SEM-005", "unit", "exact fields and predicate truth table", kModelRequirement},
}};
}

void registerSelfHostSemaModelModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_sema_support::addInfrastructurePendingRange(registry, range);
    }
}
}
