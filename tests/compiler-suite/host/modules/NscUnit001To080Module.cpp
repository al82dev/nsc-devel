#include "NscTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using nsc_support::PendingContract;

constexpr std::array<PendingContract, 10> kUnitContracts{{
    {1U,
     "automaticErrorLogEnabled is private to src/nsc/main.cpp; an environment seam is absent"},
    {2U,
     "TeeBuffer is private to src/nsc/main.cpp; controlled sink failure injection is absent"},
    {3U,
     "jsonEscape is private to src/nsc/main.cpp; no product-owned callable seam is exported"},
    {4U,
     "publishPayload is private to src/nsc/main.cpp; atomic filesystem fault injection is absent"},
    {5U,
     "publishLog is private to src/nsc/main.cpp; exact schema rendering has no callable seam"},
    {6U,
     "publishAutomaticErrorLog is private; claim collision and concurrency seams are absent"},
    {7U,
     "isCompilationInvocation is private to src/nsc/main.cpp; no callable classification seam exists"},
    {8U,
     "requestsJsonOutput is private to src/nsc/main.cpp; no callable banner-policy seam exists"},
    {9U,
     "project and library-root extraction are private; no callable option seam is exported"},
    {10U,
     "progressFormatFromArguments is private; parser injection without Driver is unavailable"},
}};
}

// Materializes TF-NSC-001..080 exactly once. Every case stays visibly blocked because duplicating
// the private product algorithms in the suite would test a surrogate rather than Host.nsc.
void registerNscUnit001To080Module(TestRegistry& registry)
{
    for (const PendingContract& contract : kUnitContracts)
    {
        nsc_support::addInfrastructureContract(registry, contract, "unit");
    }
}
}
