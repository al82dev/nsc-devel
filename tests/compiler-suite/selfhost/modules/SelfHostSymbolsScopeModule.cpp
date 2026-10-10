#include "SelfHostSymbolsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_symbols_support::PendingContractRange;

constexpr std::string_view kScopeRequirement =
    "the controller has no executable NativeSharp selfhost lexical-scope boundary with declarations, parent "
    "identity, ordered enumeration, lookup, duplicates, and nested shadowing";

constexpr std::array<PendingContractRange, 1> kContracts{{
    {938, 944, "CSH-SYM-005", "count/order/lookup and parent identity", kScopeRequirement},
}};
}

void registerSelfHostSymbolsScopeModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_symbols_support::addInfrastructurePendingRange(registry, range);
    }
}
}
