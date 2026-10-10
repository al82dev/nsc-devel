#include "SelfHostSymbolsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_symbols_support::PendingContractRange;

constexpr std::string_view kIdentityRequirement =
    "the controller has no executable NativeSharp selfhost symbol boundary exposing the complete kind "
    "vocabulary, identity fields, declaration metadata, and controlled mutation";

constexpr std::array<PendingContractRange, 2> kContracts{{
    {910, 916, "CSH-SYM-001", "exhaustive enum/name table", kIdentityRequirement},
    {917, 923, "CSH-SYM-002", "exact fields before/after mutation", kIdentityRequirement},
}};
}

void registerSelfHostSymbolsIdentityModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_symbols_support::addInfrastructurePendingRange(registry, range);
    }
}
}
