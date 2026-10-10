#include "SelfHostSyntaxTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_syntax_support::PendingContractRange;

constexpr std::string_view kDumpRequirement =
    "the controller has no executable NativeSharp selfhost syntax-dump boundary with deterministic text, "
    "malformed-state rejection, input-order perturbation, and depth observation";

constexpr std::array<PendingContractRange, 1> kContracts{{
    {974, 980, "CSH-SYN-005", "exact dump and depth stress outcome", kDumpRequirement},
}};
}

void registerSelfHostSyntaxDumpModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_syntax_support::addInfrastructurePendingRange(registry, range);
    }
}
}
