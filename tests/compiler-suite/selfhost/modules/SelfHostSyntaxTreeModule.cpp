#include "SelfHostSyntaxTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_syntax_support::PendingContractRange;

constexpr std::string_view kTreeRequirement =
    "the controller has no executable NativeSharp selfhost syntax-tree boundary with freeze transitions, "
    "structural hashing, arena identity, roots, equality, and scale fixtures";

constexpr std::array<PendingContractRange, 2> kContracts{{
    {959, 965, "CSH-SYN-003", "state transition and structural hash", kTreeRequirement},
    {966, 973, "CSH-SYN-004", "IDs count root and node equality", kTreeRequirement},
}};
}

void registerSelfHostSyntaxTreeModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_syntax_support::addInfrastructurePendingRange(registry, range);
    }
}
}
