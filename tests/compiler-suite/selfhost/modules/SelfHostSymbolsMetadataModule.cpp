#include "SelfHostSymbolsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_symbols_support::PendingContractRange;

constexpr std::string_view kMetadataRequirement =
    "the controller has no executable NativeSharp selfhost symbol boundary with member, layout, constant, "
    "flag, and reachability state observation";

constexpr std::array<PendingContractRange, 2> kContracts{{
    {924, 930, "CSH-SYM-003", "member table and exact layout values", kMetadataRequirement},
    {931, 937, "CSH-SYM-004", "flag/value transition table", kMetadataRequirement},
}};
}

void registerSelfHostSymbolsMetadataModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_symbols_support::addInfrastructurePendingRange(registry, range);
    }
}
}
