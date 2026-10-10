#include "SelfHostSemaTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_sema_support::PendingContractRange;

constexpr std::string_view kResolutionRequirement =
    "the controller has no executable NativeSharp selfhost semantic-analyzer boundary exposing scopes, "
    "generic substitution, overload candidates, layouts, conversions, and owned diagnostics";

constexpr std::array<PendingContractRange, 5> kContracts{{
    {778, 783, "CSH-SEM-011", "unit", "selected symbol scope/member tables and errors",
     kResolutionRequirement},
    {784, 791, "CSH-SEM-012", "component", "type display symbol and conversion facts",
     kResolutionRequirement},
    {792, 799, "CSH-SEM-013", "component", "candidate scores selection conversions and diagnostic",
     kResolutionRequirement},
    {800, 806, "CSH-SEM-014", "component", "symbol table layout signature constants and errors",
     kResolutionRequirement},
    {807, 812, "CSH-SEM-015", "unit", "fact types constants conversion list and error",
     kResolutionRequirement},
}};
}

void registerSelfHostSemaResolutionModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_sema_support::addInfrastructurePendingRange(registry, range);
    }
}
}
